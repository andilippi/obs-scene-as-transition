#include "obs-module.h"
#include "version.h"

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f

struct scene_as_transition {
	obs_source_t *source;
	obs_source_t *transition_scene;
	obs_source_t *filter;
	bool transitioning;
	float transition_point;
	float duration;
	const char *filter_name;

	obs_transition_audio_mix_callback_t mix_a;
	obs_transition_audio_mix_callback_t mix_b;
	float transition_a_mul;
	float transition_b_mul;
};

static const char *scene_as_transition_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("Scene");
}

static inline float calc_fade(float t, float mul)
{
	t *= mul;
	return t > 1.0f ? 1.0f : t;
}

static float mix_a_fade_in_out(void *data, float t)
{
	struct scene_as_transition *st = data;
	return 1.0f - calc_fade(t, st->transition_a_mul);
}

static float mix_b_fade_in_out(void *data, float t)
{
	struct scene_as_transition *st = data;
	return 1.0f - calc_fade(1.0f - t, st->transition_b_mul);
}

static float mix_a_cross_fade(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b_cross_fade(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

void scene_as_transition_update(void *data, obs_data_t *settings)
{
	struct scene_as_transition *st = data;

	obs_source_release(st->transition_scene);
	st->transition_scene =
		obs_get_source_by_name(obs_data_get_string(settings, "scene"));

	st->duration = (float)obs_data_get_double(settings, "duration");
	obs_transition_enable_fixed(st->source, true, (uint32_t)st->duration);

	const bool time_based_transition_point =
		obs_data_get_int(settings, "tp_type") == 1;
	if (time_based_transition_point) {
		const float transition_point_ms = (float)obs_data_get_double(
			settings, "transition_point_ms");
		if (st->duration > 0.0f)
			st->transition_point =
				transition_point_ms / st->duration;
	} else {
		st->transition_point = (float)obs_data_get_double(
					       settings, "transition_point") /
				       100.0f;
	}

	const char *filter_name = obs_data_get_string(settings, "filter");

	if (st->filter)
		obs_source_release(st->filter);

	st->filter = obs_source_get_filter_by_name(st->transition_scene,
						   filter_name);

	st->transition_a_mul = (1.0f / st->transition_point);
	st->transition_b_mul = (1.0f / (1.0f - st->transition_point));

	float def =
		(float)obs_data_get_double(settings, "audio_volume") / 100.0f;
	float db;
	if (def >= 1.0f)
		db = 0.0f;
	else if (def <= 0.0f)
		db = -INFINITY;
	else
		db = -(LOG_RANGE_DB + LOG_OFFSET_DB) *
			     powf((LOG_RANGE_DB + LOG_OFFSET_DB) /
					  LOG_OFFSET_DB,
				  -def) +
		     LOG_OFFSET_DB;
	const float mul = obs_db_to_mul(db);
	obs_source_set_volume(st->transition_scene, mul);

	if (!obs_data_get_int(settings, "audio_fade_style")) {
		st->mix_a = mix_a_fade_in_out;
		st->mix_b = mix_b_fade_in_out;
	} else {
		st->mix_a = mix_a_cross_fade;
		st->mix_b = mix_b_cross_fade;
	}
}

static void *scene_as_transition_create(obs_data_t *settings,
					obs_source_t *source)
{
	struct scene_as_transition *st;

	st = bzalloc(sizeof(*st));
	st->source = source;

	obs_transition_enable_fixed(st->source, true, 0);
	obs_source_update(source, settings);

	scene_as_transition_update(st, settings);

	return st;
}

static void scene_as_transition_destroy(void *data)
{
	struct scene_as_transition *st = data;
	obs_source_release(st->transition_scene);
	bfree(data);
}

static void scene_as_transition_video_render(void *data, gs_effect_t *effect)
{
	struct scene_as_transition *st = data;

	float t = obs_transition_get_time(st->source);
	bool use_a = t < st->transition_point;

	enum obs_transition_target target = use_a ? OBS_TRANSITION_SOURCE_A
						  : OBS_TRANSITION_SOURCE_B;

	if (!obs_transition_video_render_direct(st->source, target))
		return;

	if (t > 0.0f && t < 1.0f) {
		obs_source_video_render(st->transition_scene);
	}

	if (use_a) {
		if (!st->transitioning) {
			st->transitioning = true;
			if (obs_source_showing(st->source))
				obs_source_inc_showing(st->transition_scene);
			if (obs_source_active(st->source))
				obs_source_inc_active(st->transition_scene);
			if (st->filter)
				obs_source_set_enabled(st->filter, true);
		}
		obs_source_video_render(st->transition_scene);
	} else if ((t <= 0.0f || t >= 1.0f) && st->transitioning) {
		st->transitioning = false;
		if (obs_source_active(st->source))
			obs_source_dec_active(st->transition_scene);
		if (obs_source_showing(st->source))
			obs_source_dec_showing(st->transition_scene);
	}

	UNUSED_PARAMETER(effect);
}

static bool scene_as_transition_audio_render(void *data, uint64_t *ts_out,
					     struct obs_source_audio_mix *audio,
					     uint32_t mixers, size_t channels,
					     size_t sample_rate)
{
	struct scene_as_transition *st = data;
	if (!st)
		return false;

	uint64_t ts = 0;
	if (!obs_source_audio_pending(st->transition_scene)) {
		ts = obs_source_get_audio_timestamp(st->transition_scene);
		if (!ts)
			return false;
	}

	const bool success = obs_transition_audio_render(st->source, ts_out,
							 audio, mixers,
							 channels, sample_rate,
							 st->mix_a, st->mix_b);
	if (!ts || !st->transitioning)
		return success;

	if (!*ts_out || ts < *ts_out)
		*ts_out = ts;

	struct obs_source_audio_mix child_audio;
	obs_source_get_audio_mix(st->transition_scene, &child_audio);
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		if ((mixers & (1 << mix)) == 0)
			continue;

		for (size_t ch = 0; ch < channels; ch++) {
			register float *out = audio->output[mix].data[ch];
			register float *in = child_audio.output[mix].data[ch];
			register float *end = in + AUDIO_OUTPUT_FRAMES;

			while (in < end)
				*(out++) += *(in++);
		}
	}

	return true;
}

static enum gs_color_space scene_as_transition_video_get_color_space(
	void *data, size_t count, const enum gs_color_space *preferred_spaces)
{
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);

	struct scene_as_transition *const st = data;
	return obs_transition_video_get_color_space(st->source);
}

void scene_as_transition_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "duration", 1000.0);
	obs_data_set_default_double(settings, "transition_point", 50.0);
	obs_data_set_default_double(settings, "transition_point_ms", 500.0);
	obs_data_set_default_string(settings, "filter",
				    obs_module_text("NoFilterSelected"));
	obs_data_set_default_string(settings, "prev_scene", "");
	obs_data_set_default_double(settings, "audio_volume", 100.0);
}

static bool transition_point_type_modified(obs_properties_t *ppts,
					   obs_property_t *p, obs_data_t *s)
{
	int64_t type = obs_data_get_int(s, "tp_type");

	obs_property_t *prop_transition_point =
		obs_properties_get(ppts, "transition_point");
	obs_property_t *prop_transition_point_ms =
		obs_properties_get(ppts, "transition_point_ms");

	if (type == 1) {
		obs_property_set_visible(prop_transition_point, false);
		obs_property_set_visible(prop_transition_point_ms, true);
	} else {
		obs_property_set_visible(prop_transition_point, true);
		obs_property_set_visible(prop_transition_point_ms, false);
	}

	UNUSED_PARAMETER(p);
	return true;
}

bool scene_as_transition_list_add_scene(void *data,
					obs_source_t *transition_scene)
{
	obs_property_t *prop = data;
	const char *name = obs_source_get_name(transition_scene);
	obs_property_list_add_string(prop, name, name);
	return true;
}

void scene_as_transition_list_add_filter(obs_source_t *parent,
					 obs_source_t *child, void *data)
{
	UNUSED_PARAMETER(parent);
	obs_property_t *p = data;
	const char *name = obs_source_get_name(child);
	obs_property_list_add_string(p, name, name);
}

static bool scene_modified(obs_properties_t *props, obs_property_t *property,
			   obs_data_t *settings)
{
	obs_property_t *filter = obs_properties_get(props, "filter");
	const char *scene_name = obs_data_get_string(settings, "scene");
	const char *prev_scene_name =
		obs_data_get_string(settings, "prev_scene");

	if (strcmp(scene_name, prev_scene_name) != 0) {
		obs_source_t *scene = obs_get_source_by_name(scene_name);

		obs_property_list_clear(filter);
		obs_property_list_add_string(
			filter, obs_module_text("NoFilterSelected"), "filter");
		obs_source_enum_filters(
			scene, scene_as_transition_list_add_filter, filter);

		obs_data_set_string(settings, "filter",
				    obs_module_text("NoFilterSelected"));
		obs_data_set_string(settings, "prev_scene", scene_name);

		obs_source_release(scene);
	}

	UNUSED_PARAMETER(property);

	return true;
}

static void scene_as_transition_enum_active_sources(
	void *data, obs_source_enum_proc_t enum_callback, void *param)
{
	struct scene_as_transition *st = data;
	if (st->transition_scene && st->transitioning)
		enum_callback(st->source, st->transition_scene, param);
}

static void scene_as_transition_enum_all_sources(
	void *data, obs_source_enum_proc_t enum_callback, void *param)
{
	struct scene_as_transition *st = data;
	if (st->transition_scene)
		enum_callback(st->source, st->transition_scene, param);
}

obs_properties_t *scene_as_transition_properties(void *data)
{

	struct scene_as_transition *st = data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *scene = obs_properties_add_list(
		props, "scene", obs_module_text("Scene"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_set_long_description(scene,
					  obs_module_text("SceneDescription"));
	obs_enum_scenes(scene_as_transition_list_add_scene, scene);
	obs_property_set_modified_callback(scene, scene_modified);

	obs_property_t *p = obs_properties_add_float(
		props, "duration", obs_module_text("Duration"), 0.0, 30000.0,
		100.0);
	obs_property_float_set_suffix(p, " ms");
	obs_property_set_long_description(
		p, obs_module_text("DurationDescription"));

	obs_properties_t *transition_point_group = obs_properties_create();

	obs_properties_add_group(props, "transition_point_group",
				 obs_module_text("TransitionPointSettings"),
				 OBS_GROUP_NORMAL, transition_point_group);

	p = obs_properties_add_list(transition_point_group, "tp_type",
				    obs_module_text("TransitionPointType"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(
		p, obs_module_text("TransitionPointPercentage"), 0);
	obs_property_list_add_int(p, obs_module_text("TransitionPointTime"), 1);
	obs_property_set_long_description(
		p, obs_module_text("TransitionPointTypeDescription"));
	obs_property_set_modified_callback(p, transition_point_type_modified);
	p = obs_properties_add_float_slider(transition_point_group,
					    "transition_point",
					    obs_module_text("TransitionPoint"),
					    0, 100.0, 1.0);
	obs_property_float_set_suffix(p, "%");
	obs_property_set_long_description(
		p, obs_module_text("TransitionPointPercentageDescription"));

	p = obs_properties_add_float(transition_point_group,
				     "transition_point_ms",
				     obs_module_text("TransitionPoint"), 0,
				     30000.0, 100.0);
	obs_property_float_set_suffix(p, " ms");
	obs_property_set_long_description(
		p, obs_module_text("TransitionPointTimeDescription"));

	obs_properties_t *audio_group = obs_properties_create();

	obs_properties_add_group(props, "audio_group",
				 obs_module_text("AudioSettings"),
				 OBS_GROUP_NORMAL, audio_group);
	obs_property_t *audio_fade_style = obs_properties_add_list(
		audio_group, "audio_fade_style",
		obs_module_text("AudioFadeStyle"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_set_long_description(
		audio_fade_style, obs_module_text("AudioFadeStyleDescription"));
	obs_property_list_add_int(audio_fade_style,
				  obs_module_text("FadeOutFadeIn"), 0);
	obs_property_list_add_int(audio_fade_style,
				  obs_module_text("CrossFade"), 1);

	p = obs_properties_add_float_slider(audio_group, "audio_volume",
					    obs_module_text("AudioVolume"), 0,
					    100.0, 1.0);
	obs_property_float_set_suffix(p, "%");
	obs_property_set_long_description(
		p, obs_module_text("AudioVolumeDescription"));

	obs_property_t *filter = obs_properties_add_list(
		props, "filter", obs_module_text("FilterToTrigger"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(
		filter, obs_module_text("NoFilterSelected"), "filter");
	obs_property_set_long_description(
		filter, obs_module_text("FilterToTriggerDescription"));
	obs_source_enum_filters(st->transition_scene,
				scene_as_transition_list_add_filter, filter);

	obs_properties_add_text(
		props, "plugin_info",
		"<a href=\"https://github.com/andilippi/obs-scene-as-transition\">Scene As Transition</a> (" PROJECT_VERSION
		") by Andi Stone ( <a href=\"https://www.youtube.com/andilippi\">Andilippi</a> ) | A <a href=\"https://streamup.tips\">StreamUP</a> Product",
		OBS_TEXT_INFO);

	UNUSED_PARAMETER(data);

	return props;
}

struct obs_source_info scene_as_transition = {
	.id = "scene_as_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = scene_as_transition_get_name,
	.create = scene_as_transition_create,
	.destroy = scene_as_transition_destroy,
	.load = scene_as_transition_update,
	.update = scene_as_transition_update,
	.get_defaults = scene_as_transition_defaults,
	.enum_active_sources = scene_as_transition_enum_active_sources,
	.enum_all_sources = scene_as_transition_enum_all_sources,
	.video_render = scene_as_transition_video_render,
	.audio_render = scene_as_transition_audio_render,
	.video_get_color_space = scene_as_transition_video_get_color_space,
	.get_properties = scene_as_transition_properties,
};

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Andi Stone")
OBS_MODULE_USE_DEFAULT_LOCALE("scene-as-transition", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("SceneAsTransition");
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Scene As Transition] loaded version %s",
	     PROJECT_VERSION);
	obs_register_source(&scene_as_transition);
	return true;
}
