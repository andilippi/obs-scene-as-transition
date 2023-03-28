#include "obs-module.h"
#include "version.h"

struct scene_as_transition {
	obs_source_t *source;
	obs_source_t *transition_scene;
	bool transitioning;
	float transition_point;
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

	st->transition_point =
		(float)obs_data_get_double(settings, "transition_point") /
		100.0f;
}

static void *scene_as_transition_create(obs_data_t *settings,
					obs_source_t *source)
{
	struct scene_as_transition *st;

	st = bzalloc(sizeof(*st));
	st->source = source;

	scene_as_transition_update(st, settings);
	//obs_transition_enable_fixed(source, true, 0);

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
	UNUSED_PARAMETER(effect);
	struct scene_as_transition *st = data;

	float f = obs_transition_get_time(st->source);
	bool transitioning;
	if (f <= st->transition_point) {
		transitioning = obs_transition_video_render_direct(
			st->source, OBS_TRANSITION_SOURCE_A);
	} else {
		transitioning = obs_transition_video_render_direct(
			st->source, OBS_TRANSITION_SOURCE_B);
	}
	if (transitioning) {
		if (!st->transitioning) {
			st->transitioning = true;
			if (obs_source_showing(st->source))
				obs_source_inc_showing(st->transition_scene);
			if (obs_source_active(st->source))
				obs_source_inc_active(st->transition_scene);
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

bool scene_transition_list_add_scene(void *data, obs_source_t *transition_scene)
{
	obs_property_t *prop = data;
	obs_property_list_add_string(prop,
				     obs_source_get_name(transition_scene),
				     obs_source_get_name(transition_scene));
	return true;
}

void scene_as_transition_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "transition_point", 50.0);
}

obs_properties_t *scene_as_transition_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *scene = obs_properties_add_list(
		props, "scene", "Scene", OBS_COMBO_TYPE_EDITABLE,
		OBS_COMBO_FORMAT_STRING);
	obs_enum_scenes(scene_transition_list_add_scene, scene);

	obs_property_t *p = obs_properties_add_float_slider(
		props, "transition_point", obs_module_text("TransitionPoint"),
		0, 100.0, 1.0);
	obs_property_float_set_suffix(p, "%");

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
