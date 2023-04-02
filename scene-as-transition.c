#include "obs-module.h"
#include "version.h"

struct scene_as_transition {
	obs_source_t *source;
	obs_source_t *transition_scene;
	obs_source_t *filter;
	bool transitioning;
	float transition_point;
	float duration;
	const char *filter_name;
};

static const char *scene_as_transition_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("Scene");
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
}

static void *scene_as_transition_create(obs_data_t *settings,
					obs_source_t *source)
{
	struct scene_as_transition *st;

	st = bzalloc(sizeof(*st));
	st->source = source;

	scene_as_transition_update(st, settings);

	return st;
}

static void scene_as_transition_destroy(void *data)
{
	struct scene_as_transition *st = data;
	obs_source_release(st->transition_scene);
	bfree(st);
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
	} else if (st->transitioning) {
		st->transitioning = false;
		if (obs_source_active(st->source))
			obs_source_dec_active(st->transition_scene);
		if (obs_source_showing(st->source))
			obs_source_dec_showing(st->transition_scene);
	}

	UNUSED_PARAMETER(effect);
}


static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

static bool scene_as_transition_audio_render(void *data, uint64_t *ts_out,
					     struct obs_source_audio_mix *audio,
					     uint32_t mixers, size_t channels,
					     size_t sample_rate)
{
	struct scene_as_transition *st = data;
	return obs_transition_audio_render(st->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
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
	obs_data_set_default_string(settings, "filter", "No Filter Selected");
	obs_data_set_default_string(settings, "prev_scene", "");
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
			filter, obs_module_text("No Filter Selected"),
			"filter");
		obs_source_enum_filters(
			scene, scene_as_transition_list_add_filter, filter);

		obs_data_set_string(settings, "filter", "No Filter Selected");
		obs_data_set_string(settings, "prev_scene", scene_name);

		obs_source_release(scene);
	}

	UNUSED_PARAMETER(property);

	return true;
}

obs_properties_t *scene_as_transition_properties(void *data)
{

	struct scene_as_transition *st = data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *scene = obs_properties_add_list(
		props, "scene", "Scene", OBS_COMBO_TYPE_EDITABLE,
		OBS_COMBO_FORMAT_STRING);
	obs_property_set_long_description(
		scene,
		obs_module_text(
			"Select the scene you wish to use as the transition."));
	obs_enum_scenes(scene_as_transition_list_add_scene, scene);
	obs_property_set_modified_callback(scene, scene_modified);

	obs_property_t *p = obs_properties_add_float(
		props, "duration", obs_module_text("Duration"), 0.0, 30000.0,
		100.0);
	obs_property_float_set_suffix(p, " ms");
	obs_property_set_long_description(
		p,
		obs_module_text(
			"The total duration of the transition in milliseconds."));

	obs_properties_t *transition_point_group = obs_properties_create();

	obs_properties_add_group(props, "transition_point_group",
				 obs_module_text("Transition Point"),
				 OBS_GROUP_NORMAL, transition_point_group);

	p = obs_properties_add_list(transition_point_group, "tp_type",
				    obs_module_text("Type"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("Percentage"), 0);
	obs_property_list_add_int(p, obs_module_text("Time"), 1);
	obs_property_set_long_description(
		p,
		obs_module_text(
			"Select the type of transition point (percentage or time)."));
	obs_property_set_modified_callback(p, transition_point_type_modified);
	p = obs_properties_add_float_slider(transition_point_group,
					    "transition_point",
					    obs_module_text("Transition Point"),
					    0, 100.0, 1.0);
	obs_property_float_set_suffix(p, "%");
	obs_property_set_long_description(
		p,
		obs_module_text(
			"The position of the transition point as a percentage of the total duration."));

	p = obs_properties_add_float(transition_point_group,
				     "transition_point_ms",
				     obs_module_text("Transition Point"), 0,
				     30000.0, 100.0);
	obs_property_float_set_suffix(p, " ms");
	obs_property_set_long_description(
		p,
		obs_module_text(
			"The position of the transition point in milliseconds."));

	obs_property_t *filter = obs_properties_add_list(
		props, "filter", obs_module_text("Filter To Trigger"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(
		filter, obs_module_text("No Filter Selected"), "filter");
	obs_property_set_long_description(
		filter,
		obs_module_text(
			"Selecting a filter here will enable it when the transition begins."));
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
