/*
	Copyright (C) 2008 - 2023
	by Tomasz Sniatowski <kailoran@gmail.com>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-editor"

#include "editor/controller/editor_controller.hpp"

#include "gettext.hpp"
#include "gui/dialogs/editor/choose_addon.hpp"
#include "gui/dialogs/message.hpp"
#include "filesystem.hpp"
#include "editor/action/action_base.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"

lg::log_domain log_editor("editor");

namespace editor {

void initialize_addon(const std::string& addon_id)
{
	std::string addon_dir = filesystem::get_addons_dir() + "/" + addon_id;

	// create folders
	filesystem::create_directory_if_missing(addon_dir);
	filesystem::create_directory_if_missing(addon_dir + "/maps");
	filesystem::create_directory_if_missing(addon_dir + "/scenarios");
	filesystem::create_directory_if_missing(addon_dir + "/images");
	filesystem::create_directory_if_missing(addon_dir + "/images/units");
	filesystem::create_directory_if_missing(addon_dir + "/masks");
	filesystem::create_directory_if_missing(addon_dir + "/music");
	filesystem::create_directory_if_missing(addon_dir + "/sounds");
	filesystem::create_directory_if_missing(addon_dir + "/translations");
	filesystem::create_directory_if_missing(addon_dir + "/units");
	filesystem::create_directory_if_missing(addon_dir + "/utils");

	// create files
	// achievements
	{
		filesystem::scoped_ostream stream = filesystem::ostream_file(addon_dir + "/achievements.cfg");
		*stream << "";
	}

	// _server.pbl
	{
		filesystem::scoped_ostream stream = filesystem::ostream_file(addon_dir + "/_server.pbl");
		*stream << "";
	}

	// a basic _main.cfg
	{
		filesystem::scoped_ostream stream = filesystem::ostream_file(addon_dir + "/_main.cfg");
		*stream << "#textdomain wesnoth-" << addon_id << "\n"
				<< "[textdomain]" << "\n"
				<< "    name=\"wesnoth-" << addon_id << "\"\n"
				<< "    path=\"data/add-ons/" << addon_id << "/translations\"\n"
				<< "[/textdomain]\n"
				<< "\n"
				<< "#ifdef MULTIPLAYER\n"
				<< "[binary_path]\n"
				<< "    path=data/add-ons/" << addon_id << "\n"
				<< "[/binary_path]\n"
				<< "\n"
				<< "{~add-ons/" << addon_id << "/scenarios}\n"
				<< "{~add-ons/" << addon_id << "/utils}\n"
				<< "\n"
				<< "[units]\n"
				<< "    {~add-ons/" << addon_id << "/units}\n"
				<< "[/units]\n"
				<< "#endif\n";
	}
}

EXIT_STATUS start(bool clear_id, const std::string& filename, bool take_screenshot, const std::string& screenshot_filename)
{
	EXIT_STATUS e = EXIT_ERROR;
	try {
		const hotkey::scope_changer h{hotkey::scope_editor};

		std::string addon_id = "";
		if(clear_id) {
			while(true)
			{
				gui2::dialogs::editor_choose_addon choose(addon_id);
				if(choose.show()) {
					break;
				} else {
					return EXIT_STATUS::EXIT_NORMAL;
				}
			}

			if(addon_id == "///newaddon///") {
				std::int64_t current_millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				addon_id = "MyAwesomeAddon-"+std::to_string(current_millis);
			}
		} else {
			addon_id = editor_controller::current_addon_id_;
		}
		editor_controller editor(addon_id);

		// don't let people try to migrate their editor stuff into mainline folders
		if(!take_screenshot && addon_id != "mainline") {
			std::string addon_dir = filesystem::get_addons_dir() + "/" + addon_id;
			if(!filesystem::file_exists(addon_dir)) {
				initialize_addon(addon_id);
			}
		}

		if (!filename.empty() && filesystem::file_exists (filename)) {
			if (filesystem::is_directory(filename)) {
				editor.context_manager_->set_default_dir(filename);
				editor.context_manager_->load_map_dialog(true);
			} else {
				editor.context_manager_->load_map(filename, false);

				// HACK: this fixes an issue where the button overlays would be missing when
				// the loaded map appears. Since we're gonna drop this ridiculous GUI1 drawing
				// stuff in 1.15 I'm not going to waste time coming up with a better fix.
				//
				// Do note adding a redraw_everything call to context_manager::refresh_all also
				// fixes the issue, but I'm pretty sure thats just because editor_controller::
				// display_redraw_callback gets called, which then calls set_button_state.
				//
				// -- vultraz, 2018-02-24
				editor.set_button_state();
			}

			if (take_screenshot) {
				editor.do_screenshot(screenshot_filename);
				e = EXIT_NORMAL;
			}
		}

		if (!take_screenshot) {
			e = editor.main_loop();
		}
	} catch(const editor_exception& e) {
		ERR_ED << "Editor exception in editor::start: " << e.what();
		throw;
	}
	if (editor_action::get_instance_count() != 0) {
		ERR_ED << "Possibly leaked " << editor_action::get_instance_count() << " action objects";
	}

	return e;
}

} //end namespace editor
