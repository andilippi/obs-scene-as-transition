#include "obs-module.h"
#include "version.h"
#include <util/platform.h>
#include <util/dstr.h>
#include <obs-frontend-api.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f

struct scene_as_transition {
	obs_source_t *source;
	obs_source_t *transition_scene;
	obs_source_t *filter;
	bool transitioning;
	float transition_point;
	float duration;
	char *filter_name;

	obs_transition_audio_mix_callback_t mix_a;
	obs_transition_audio_mix_callback_t mix_b;
	float transition_a_mul;
	float transition_b_mul;
};

static const char *scene_as_transition_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("Plugin.Name");
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
	if (!st)
		return;

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

	// Check if filter name has changed to avoid unnecessary re-fetching
	bool filter_name_changed = !st->filter_name ||
				   strcmp(st->filter_name, filter_name) != 0;

	// Store filter name for lazy loading
	if (filter_name_changed) {
		if (st->filter_name)
			bfree(st->filter_name);
		st->filter_name = bstrdup(filter_name);

		// Release existing filter only if name changed
		if (st->filter) {
			obs_source_release(st->filter);
			st->filter = NULL;
		}

		// Check if a valid filter is selected (not "NoFilterSelected" or empty)
		const char *no_filter_text = obs_module_text("Filter.NoSelection");
		bool has_valid_filter = filter_name && *filter_name &&
					strcmp(filter_name, no_filter_text) != 0 &&
					strcmp(filter_name, "filter") != 0;

		if (has_valid_filter && st->transition_scene) {
			st->filter = obs_source_get_filter_by_name(st->transition_scene,
								   filter_name);
			if (!st->filter) {
				blog(LOG_WARNING,
				     "[StreamUP Scene as Transition] Failed to find filter '%s' on scene '%s'. "
				     "Filter may not be loaded yet and will be retried during transition.",
				     filter_name, obs_source_get_name(st->transition_scene));
			} else {
				blog(LOG_INFO,
				     "[StreamUP Scene as Transition] Successfully loaded filter '%s' from scene '%s'",
				     filter_name, obs_source_get_name(st->transition_scene));
			}
		}
	}

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

	// Ensure transitioning is set to true initially
	if (!st->transitioning) {
		st->transitioning = true;
	}
}

static void *scene_as_transition_create(obs_data_t *settings,
					obs_source_t *source)
{
	struct scene_as_transition *st;

	st = bzalloc(sizeof(*st));
	st->source = source;

	// Initialize transitioning to true
	st->transitioning = true;

	obs_transition_enable_fixed(st->source, true, 0);
	obs_source_update(source, settings);

	scene_as_transition_update(st, settings);

	// Set initial audio mix callbacks
	st->mix_a = mix_a_fade_in_out;
	st->mix_b = mix_b_fade_in_out;

	return st;
}

static void scene_as_transition_destroy(void *data)
{
	struct scene_as_transition *st = data;
	obs_source_release(st->transition_scene);
	if (st->filter)
		obs_source_release(st->filter);
	if (st->filter_name)
		bfree(st->filter_name);
	bfree(data);
}

static void scene_as_transition_video_render(void *data, gs_effect_t *effect)
{
	struct scene_as_transition *st = data;

	// NULL safety check
	if (!st || !st->transition_scene) {
		return;
	}

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

			// Lazy load filter if it wasn't available during init
			if (!st->filter && st->filter_name && st->transition_scene) {
				const char *no_filter_text = obs_module_text("Filter.NoSelection");
				bool has_valid_filter = st->filter_name && *st->filter_name &&
							strcmp(st->filter_name, no_filter_text) != 0 &&
							strcmp(st->filter_name, "filter") != 0;

				if (has_valid_filter) {
					st->filter = obs_source_get_filter_by_name(
						st->transition_scene, st->filter_name);
					if (st->filter) {
						blog(LOG_INFO,
						     "[StreamUP Scene as Transition] Lazy loading succeeded: "
						     "Found filter '%s' on scene '%s'",
						     st->filter_name,
						     obs_source_get_name(st->transition_scene));
					} else {
						blog(LOG_WARNING,
						     "[StreamUP Scene as Transition] Lazy loading failed: "
						     "Filter '%s' still not found on scene '%s'",
						     st->filter_name,
						     obs_source_get_name(st->transition_scene));
					}
				}
			}

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

		// Disable filter when transition ends
		if (st->filter)
			obs_source_set_enabled(st->filter, false);
	}

	UNUSED_PARAMETER(effect);
}

static bool scene_as_transition_audio_render(void *data, uint64_t *ts_out,
					     struct obs_source_audio_mix *audio,
					     uint32_t mixers, size_t channels,
					     size_t sample_rate)
{
	struct scene_as_transition *st = data;
	if (!st || !st->transition_scene)
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
				    obs_module_text("Filter.NoSelection"));
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
			filter, obs_module_text("Filter.NoSelection"), "filter");
		obs_source_enum_filters(
			scene, scene_as_transition_list_add_filter, filter);

		obs_data_set_string(settings, "filter",
				    obs_module_text("Filter.NoSelection"));
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
		props, "scene", obs_module_text("Scene.Name"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_set_long_description(scene,
					  obs_module_text("Scene.Description"));
	obs_enum_scenes(scene_as_transition_list_add_scene, scene);
	obs_property_set_modified_callback(scene, scene_modified);

	obs_property_t *p = obs_properties_add_float(
		props, "duration", obs_module_text("Transition.Duration"), 0.0, 30000.0,
		100.0);
	obs_property_float_set_suffix(p, " ms");
	obs_property_set_long_description(
		p, obs_module_text("Transition.Duration.Description"));

	obs_properties_t *transition_point_group = obs_properties_create();

	obs_properties_add_group(props, "transition_point_group",
				 obs_module_text("TransitionPoint.Settings"),
				 OBS_GROUP_NORMAL, transition_point_group);

	p = obs_properties_add_list(transition_point_group, "tp_type",
				    obs_module_text("TransitionPoint.Type"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(
		p, obs_module_text("TransitionPoint.Percentage"), 0);
	obs_property_list_add_int(p, obs_module_text("TransitionPoint.Time"), 1);
	obs_property_set_long_description(
		p, obs_module_text("TransitionPoint.Type.Description"));
	obs_property_set_modified_callback(p, transition_point_type_modified);
	p = obs_properties_add_float_slider(transition_point_group,
					    "transition_point",
					    obs_module_text("TransitionPoint.Name"),
					    0, 100.0, 1.0);
	obs_property_float_set_suffix(p, "%");
	obs_property_set_long_description(
		p, obs_module_text("TransitionPoint.Percentage.Description"));

	p = obs_properties_add_float(transition_point_group,
				     "transition_point_ms",
				     obs_module_text("TransitionPoint.Name"), 0,
				     30000.0, 100.0);
	obs_property_float_set_suffix(p, " ms");
	obs_property_set_long_description(
		p, obs_module_text("TransitionPoint.Time.Description"));

	obs_properties_t *audio_group = obs_properties_create();

	obs_properties_add_group(props, "audio_group",
				 obs_module_text("Audio.Settings"),
				 OBS_GROUP_NORMAL, audio_group);
	obs_property_t *audio_fade_style = obs_properties_add_list(
		audio_group, "audio_fade_style",
		obs_module_text("Audio.FadeStyle"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_set_long_description(
		audio_fade_style, obs_module_text("Audio.FadeStyle.Description"));
	obs_property_list_add_int(audio_fade_style,
				  obs_module_text("Audio.FadeStyle.FadeOutIn"), 0);
	obs_property_list_add_int(audio_fade_style,
				  obs_module_text("Audio.FadeStyle.CrossFade"), 1);

	p = obs_properties_add_float_slider(audio_group, "audio_volume",
					    obs_module_text("Audio.Volume"), 0,
					    100.0, 1.0);
	obs_property_float_set_suffix(p, "%");
	obs_property_set_long_description(
		p, obs_module_text("Audio.Volume.Description"));

	obs_property_t *filter = obs_properties_add_list(
		props, "filter", obs_module_text("Filter.ToTrigger"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(
		filter, obs_module_text("Filter.NoSelection"), "filter");
	obs_property_set_long_description(
		filter, obs_module_text("Filter.ToTrigger.Description"));
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
	return obs_module_text("Plugin.Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("Plugin.Name");
}

static void open_folder_and_highlight(const char *file_path)
{
#ifdef _WIN32
	// Convert forward slashes to backslashes for Windows
	struct dstr windows_path = {0};
	dstr_copy(&windows_path, file_path);
	dstr_replace(&windows_path, "/", "\\");

	// Build the explorer command to select the file
	struct dstr command = {0};
	dstr_printf(&command, "/select,\"%s\"", windows_path.array);

	// Open explorer and highlight the file
	ShellExecuteA(NULL, "open", "explorer.exe", command.array, NULL,
		      SW_SHOWNORMAL);

	dstr_free(&command);
	dstr_free(&windows_path);
#elif __APPLE__
	// macOS: Use 'open -R' to reveal in Finder
	struct dstr command = {0};
	dstr_printf(&command, "open -R \"%s\"", file_path);
	int result = system(command.array);
	if (result != 0) {
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] Failed to open Finder");
	}
	dstr_free(&command);
#else
	// Linux: Open the parent directory
	struct dstr dir_path = {0};
	dstr_copy(&dir_path, file_path);
	char *last_slash = strrchr(dir_path.array, '/');
	if (last_slash) {
		*last_slash = '\0';
		struct dstr command = {0};
		dstr_printf(&command, "xdg-open \"%s\"", dir_path.array);
		int result = system(command.array);
		if (result != 0) {
			blog(LOG_WARNING,
			     "[StreamUP Scene as Transition] Failed to open file manager");
		}
		dstr_free(&command);
	}
	dstr_free(&dir_path);
#endif
}

struct old_plugin_check_data {
	char *old_plugin_path;
};

static void show_old_plugin_dialog(void *data)
{
	struct old_plugin_check_data *check_data = data;
	if (!check_data || !check_data->old_plugin_path)
		return;

#ifdef _WIN32
	// Create message for the dialog
	struct dstr message = {0};
	dstr_printf(&message,
		    "An old version of Scene as Transition has been detected:\n\n"
		    "%s\n\n"
		    "This old version may cause conflicts with the new StreamUP Scene as Transition plugin.\n\n"
		    "Would you like to open the plugins folder to remove it?",
		    check_data->old_plugin_path);

	// Show message box with Yes/No buttons
	int result = MessageBoxA(NULL, message.array,
				 "StreamUP Scene as Transition - Old Plugin Detected",
				 MB_YESNO | MB_ICONWARNING | MB_TOPMOST);

	if (result == IDYES) {
		open_folder_and_highlight(check_data->old_plugin_path);
	}

	dstr_free(&message);
#else
	// For non-Windows, just open the folder
	open_folder_and_highlight(check_data->old_plugin_path);
#endif

	// Clean up
	bfree(check_data->old_plugin_path);
	bfree(check_data);
}

static void find_old_plugin_files(struct dstr *found_path)
{
	// Get the binary module path
	const char *bin_path = obs_get_module_binary_path(obs_current_module());
	if (!bin_path) {
		blog(LOG_INFO,
		     "[StreamUP Scene as Transition] Unable to get module binary path");
		return;
	}

	blog(LOG_INFO,
	     "[StreamUP Scene as Transition] Current module binary path: %s",
	     bin_path);

	struct dstr plugin_dir = {0};
	dstr_init_copy(&plugin_dir, bin_path);

	// Navigate to the plugins directory
	char *last_slash = strrchr(plugin_dir.array, '/');
	if (!last_slash)
		last_slash = strrchr(plugin_dir.array, '\\');

	if (!last_slash) {
		blog(LOG_INFO,
		     "[StreamUP Scene as Transition] Unable to determine plugin directory");
		dstr_free(&plugin_dir);
		return;
	}

	// Truncate to directory
	dstr_resize(&plugin_dir, last_slash - plugin_dir.array + 1);

	blog(LOG_INFO,
	     "[StreamUP Scene as Transition] Checking for old plugin in: %s",
	     plugin_dir.array);

	// List of possible old plugin names to check
	const char *old_plugin_names[] = {"scene-as-transition.dll",
					  "obs-scene-as-transition.dll",
					  "SceneAsTransition.dll", NULL};

	struct dstr old_plugin_path = {0};

	for (int i = 0; old_plugin_names[i] != NULL; i++) {
		dstr_copy(&old_plugin_path, plugin_dir.array);
		dstr_cat(&old_plugin_path, old_plugin_names[i]);

		blog(LOG_INFO,
		     "[StreamUP Scene as Transition] Checking for: %s",
		     old_plugin_path.array);

		if (os_file_exists(old_plugin_path.array)) {
			dstr_copy(found_path, old_plugin_path.array);
			blog(LOG_INFO,
			     "[StreamUP Scene as Transition] Found old plugin file at: %s",
			     old_plugin_path.array);
			dstr_free(&old_plugin_path);
			dstr_free(&plugin_dir);
			return;
		}
	}

	blog(LOG_INFO,
	     "[StreamUP Scene as Transition] No old plugin file found in binary directory");

	dstr_free(&old_plugin_path);
	dstr_free(&plugin_dir);
}

static void check_for_old_plugin(void)
{
	// First check if the source is already registered (indicates old plugin is loaded)
	const char *source_id = "scene_as_transition";
	bool source_already_exists = false;

	// Try to get the source output flags - if the source type is registered, this returns non-zero
	uint32_t flags = obs_get_source_output_flags(source_id);
	if (flags != 0) {
		source_already_exists = true;
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] =========================================");
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] OLD PLUGIN DETECTED!");
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] The source ID '%s' is already registered.",
		     source_id);
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] This indicates the old 'Scene As Transition' plugin");
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] is currently loaded and must be removed.");
		blog(LOG_WARNING,
		     "[StreamUP Scene as Transition] =========================================");
	}

	// Try to find the actual file
	struct dstr old_plugin_path = {0};
	find_old_plugin_files(&old_plugin_path);

	// Show dialog if we detected the old plugin
	if (source_already_exists) {
		struct old_plugin_check_data *check_data =
			bzalloc(sizeof(struct old_plugin_check_data));

		// Use the found file path if available, otherwise just indicate we found it
		if (!dstr_is_empty(&old_plugin_path)) {
			check_data->old_plugin_path =
				bstrdup(old_plugin_path.array);
		} else {
			// We know it's loaded but couldn't find the file
			// Provide a generic message
			check_data->old_plugin_path = bstrdup(
				"Old plugin is loaded but file location could not be determined");
		}

		// Queue task to show dialog on UI thread
		obs_queue_task(OBS_TASK_UI, show_old_plugin_dialog, check_data,
			       false);
	}

	dstr_free(&old_plugin_path);
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[StreamUP Scene as Transition] loaded version %s",
	     PROJECT_VERSION);

	// Check for old plugin version
	check_for_old_plugin();

	obs_register_source(&scene_as_transition);
	return true;
}
