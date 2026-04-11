#include "json/json.hpp"
#include "ui_mod_marketplace.h"
#include "librecomp/game.hpp"
#include "recompui/recompui.h"
#include "ui_utils.h"

extern std::vector<recomp::GameEntry> supported_games;

namespace recompui
{
    // Gets the URL for the marketplace so we can determine if we have to render the
    // button for the UI
    std::string get_marketplace_url()
    {
        return supported_games.empty() ? "" : supported_games[0].marketplace_url;
    }

    // Each Mod Entry on the UI
    ModMarketplaceEntry::ModMarketplaceEntry(Element *parent,
                                             const MarketplaceMod &mod_data)
        : Element(parent, Events(EventType::Click)), mod_data(mod_data)
    {
        ContextId context = get_current_context();

        entry_container = context.create_element<Container>(
            this, FlexDirection::Row, JustifyContent::FlexStart);
        entry_container->set_width(100.0f, Unit::Percent);
        entry_container->set_padding(16.0f);
        entry_container->set_margin_bottom(12.0f);
        entry_container->set_background_color(Color{26, 24, 32, 255});
        entry_container->set_border_radius(8.0f);
        entry_container->set_border_width(1.0f);
        entry_container->set_border_color(Color{242, 242, 242, 64});

        thumbnail_image = context.create_element<Image>(entry_container, "");
        thumbnail_image->set_width(80.0f, Unit::Px);
        thumbnail_image->set_height(80.0f, Unit::Px);
        thumbnail_image->set_background_color(Color{190, 184, 219, 100});
        thumbnail_image->set_border_radius(4.0f);
        thumbnail_image->set_margin_right(16.0f);

        // if we actually have a thumbnail, check if its base64 and if we need to
        // decode it or not.
        if (!mod_data.thumbnail_image.empty())
        {
            if (mod_data.thumbnail_image.starts_with("data:image/"))
            {
                std::string thumbnail_src = "marketplace_thumb_" + mod_data.id;
                std::vector<char> image_data = decode_base64(mod_data.thumbnail_image);
                if (!image_data.empty())
                {
                    recompui::queue_image_from_bytes_file(thumbnail_src, image_data);
                    thumbnail_image->set_src(thumbnail_src);
                }
            }
            else
            {
                thumbnail_image->set_src(mod_data.thumbnail_image);
            }
        }

        Container *content_container = context.create_element<Container>(
            entry_container, FlexDirection::Column, JustifyContent::FlexStart);
        content_container->set_flex(1.0f, 1.0f);
        content_container->set_gap(8.0f);

        name_label = context.create_element<Label>(content_container, mod_data.name,
                                                   LabelStyle::Normal);
        name_label->set_color(Color{242, 242, 242, 255});
        name_label->set_font_weight(600);

        description_label = context.create_element<Label>(
            content_container, mod_data.short_description, LabelStyle::Small);
        description_label->set_color(Color{204, 204, 204, 255});

        // If we have dependencies list all of them
        if (!mod_data.dependencies.empty())
        {
            std::string dep_text = "Requires: ";
            for (size_t i = 0; i < mod_data.dependencies.size(); i++)
            {
                auto [dep_id, dep_ver] = parse_dep_string(mod_data.dependencies[i]);
                if (i > 0)
                    dep_text += ", ";
                dep_text += dep_id;
                if (!dep_ver.empty())
                    dep_text += " v" + dep_ver;
            }
            Label *deps_label = context.create_element<Label>(
                content_container, dep_text, LabelStyle::Small);
            deps_label->set_color(Color{180, 150, 100, 255});
        }

        Container *button_container = context.create_element<Container>(
            entry_container, FlexDirection::Column, JustifyContent::Center);
        download_button = context.create_element<Button>(button_container, "Download",
                                                         ButtonStyle::Primary);
        download_button->add_pressed_callback([this]()
                                              {
    if (download_callback)
      download_callback(this->mod_data); });
    }

    ModMarketplaceEntry::~ModMarketplaceEntry() {}

    // Sets the callback for the click event
    void ModMarketplaceEntry::set_download_callback(
        std::function<void(const MarketplaceMod &)> callback)
    {
        download_callback = callback;
    }

    // Sets the status for the downlaod button based on the mod state.
    void ModMarketplaceEntry::update_install_status(ModInstallStatus status)
    {
        switch (status)
        {
        case ModInstallStatus::NotInstalled:
            download_button->set_text("Download");
            download_button->set_enabled(true);
            break;

        case ModInstallStatus::Installed:
            download_button->set_text("Installed");
            download_button->set_enabled(false);
            download_button->set_background_color(Color{100, 100, 100, 255});
            break;

        case ModInstallStatus::UpdateAvailable:
            download_button->set_text("Update Available");
            download_button->set_enabled(true);
            download_button->set_background_color(Color{50, 150, 50, 255});
            break;

        case ModInstallStatus::DowngradeAvailable:
            download_button->set_text("Downgrade Available");
            download_button->set_enabled(true);
            download_button->set_background_color(Color{150, 150, 50, 255});
            break;

        case ModInstallStatus::MissingDependencies:
            download_button->set_text("Missing Dependencies");
            download_button->set_enabled(false);
            download_button->set_background_color(Color{120, 60, 60, 255});
            break;
        }

        download_button->queue_update();
        queue_update();
    }

    void ModMarketplaceEntry::process_event(const Event & /*e*/) {}

    // Initializes the marketplace modal panel and child controls.
    ModDownloadsPanel::ModDownloadsPanel(Element *parent)
        : Element(parent, Events(EventType::Update))
    {
        ContextId context = get_current_context();

        set_display(Display::None);
        set_position(Position::Absolute);
        set_top(0.0f, Unit::Px);
        set_left(0.0f, Unit::Px);
        set_width(100.0f, Unit::Percent);
        set_height(100.0f, Unit::Percent);
        set_background_color(Color{0, 0, 0, 200});

        main_container = context.create_element<Container>(
            this, FlexDirection::Column, JustifyContent::Center);
        main_container->set_align_items(AlignItems::Center);
        main_container->set_width(100.0f, Unit::Percent);
        main_container->set_height(100.0f, Unit::Percent);

        content_panel = context.create_element<Container>(
            main_container, FlexDirection::Column, JustifyContent::FlexStart);
        content_panel->set_width(90.0f, Unit::Percent);
        content_panel->set_height(85.0f, Unit::Percent);
        content_panel->set_background_color(Color{26, 24, 32, 255});
        content_panel->set_border_radius(16.0f);
        content_panel->set_padding(30.0f);
        content_panel->set_gap(20.0f);
        content_panel->set_border_width(2.0f);
        content_panel->set_border_color(Color{242, 242, 242, 64});

        Container *header_container = context.create_element<Container>(
            content_panel, FlexDirection::Row, JustifyContent::SpaceBetween);
        header_container->set_align_items(AlignItems::Center);
        header_container->set_width(100.0f, Unit::Percent);

        title_label = context.create_element<Label>(
            header_container, "Mod Marketplace", LabelStyle::Large);
        title_label->set_color(Color{242, 242, 242, 255});

        refresh_button = context.create_element<Button>(header_container, "Refresh",
                                                        ButtonStyle::Secondary);
        refresh_button->add_pressed_callback([this]()
                                             { fetch_marketplace_data(); });

        status_label = context.create_element<Label>(
            content_panel, "Loading marketplace data...", LabelStyle::Normal);
        status_label->set_color(Color{204, 204, 204, 255});
        status_label->set_text_align(TextAlign::Center);

        mod_list_container = context.create_element<ScrollContainer>(
            content_panel, ScrollDirection::Vertical);
        mod_list_container->set_flex(1.0f, 1.0f);
        mod_list_container->set_width(100.0f, Unit::Percent);

        Container *button_container = context.create_element<Container>(
            content_panel, FlexDirection::Row, JustifyContent::Center);
        button_container->set_align_items(AlignItems::Center);
        close_button = context.create_element<Button>(button_container, "Close",
                                                      ButtonStyle::Secondary);
        close_button->add_pressed_callback([this]()
                                           { hide(); });

        close_button->set_nav(NavDirection::Up, refresh_button);
        close_button->set_nav(NavDirection::Down, refresh_button);
        close_button->set_nav_none(NavDirection::Left);
        close_button->set_nav_none(NavDirection::Right);
        refresh_button->set_nav(NavDirection::Up, close_button);
        refresh_button->set_nav(NavDirection::Down, close_button);
        refresh_button->set_nav_none(NavDirection::Left);
        refresh_button->set_nav_none(NavDirection::Right);
    }

    ModDownloadsPanel::~ModDownloadsPanel() {}

    // Shows the modal panel and triggers a data refresh.
    void ModDownloadsPanel::show()
    {
        set_display(Display::Flex);
        is_visible = true;
        fetch_marketplace_data();
        close_button->focus();
    }

    // Hides the modal panel.
    void ModDownloadsPanel::hide()
    {
        set_display(Display::None);
        is_visible = false;
    }

    // Fetches marketplace JSON and refreshes rendered entries.
    void ModDownloadsPanel::fetch_marketplace_data()
    {
        if (is_loading)
            return;

        is_loading = true;
        status_label->set_text("Loading marketplace data...");
        mod_list_container->clear_children();
        mod_entries.clear();

        try
        {
            std::string marketplace_url = get_marketplace_url();
            if (marketplace_url.empty())
                throw std::runtime_error("No marketplace URL configured for this game");
            std::string json_data = http_fetch_string(marketplace_url);
            printf("[ModDownloads] JSON response:\n%s\n", json_data.c_str());
            std::vector<MarketplaceMod> mods = parse_marketplace_json(json_data);
            fetched_mods = mods;
            load_marketplace_mods(mods);
        }
        catch (const std::exception &e)
        {
            status_label->set_text("Failed to load marketplace data: " +
                                   std::string(e.what()));
        }

        is_loading = false;
    }

    // Parses marketplace JSON payload into mod metadata entries.
    std::vector<MarketplaceMod>
    ModDownloadsPanel::parse_marketplace_json(const std::string &json_data)
    {
        if (json_data.empty())
            throw std::runtime_error("Empty JSON data received");

        std::vector<MarketplaceMod> mods;

        try
        {
            nlohmann::json j = nlohmann::json::parse(json_data);

            if (j.empty())
                throw std::runtime_error("JSON data is empty object");

            for (auto &[mod_name, mod_info] : j.items())
            {
                MarketplaceMod mod;
                mod.name = mod_name;

                auto try_get = [&](const char *key, std::string &field)
                {
                    if (mod_info.contains(key))
                        field = mod_info[key];
                };

                try_get("short_description", mod.short_description);
                try_get("file_url", mod.file_url);
                try_get("thumbnail_image", mod.thumbnail_image);
                try_get("version", mod.version);
                try_get("id", mod.id);
                try_get("game_id", mod.game_id);

                if (mod_info.contains("dependencies") &&
                    mod_info["dependencies"].is_array())
                {
                    for (const auto &dep : mod_info["dependencies"])
                        mod.dependencies.push_back(dep);
                }

                mods.push_back(mod);
            }
        }
        catch (const nlohmann::json::exception &e)
        {
            throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
        }

        if (mods.empty())
            throw std::runtime_error("No valid mods found in JSON");

        return mods;
    }

    // Loads filtered marketplace mods into the ui
    void ModDownloadsPanel::load_marketplace_mods(
        const std::vector<MarketplaceMod> &mods)
    {
        ContextId context = get_current_context();

        mod_list_container->clear_children();
        mod_entries.clear();

        if (mods.empty())
        {
            status_label->set_text("No mods available in marketplace");
            return;
        }

        std::string current_game_id =
            supported_games.empty() ? "" : supported_games[0].mod_game_id;

        std::vector<MarketplaceMod> filtered_mods;
        for (const auto &mod : mods)
        {
            if (mod.game_id.empty() || mod.game_id == current_game_id)
                filtered_mods.push_back(mod);
        }

        if (filtered_mods.empty())
        {
            status_label->set_text("No mods available for this game in marketplace");
            return;
        }

        status_label->set_text("Found " + std::to_string(filtered_mods.size()) +
                               " mod(s) in marketplace");

        for (const auto &mod : filtered_mods)
        {
            try
            {
                ModMarketplaceEntry *entry =
                    context.create_element<ModMarketplaceEntry>(mod_list_container, mod);
                entry->set_download_callback(
                    [this](const MarketplaceMod &m)
                    { download_mod(m); });
                entry->update_install_status(get_mod_install_status(mod));
                entry->get_download_button()->set_nav_none(NavDirection::Left);
                entry->get_download_button()->set_nav_none(NavDirection::Right);
                mod_entries.push_back(entry);
            }
            catch (const std::exception &e)
            {
                status_label->set_text("Error creating mod entry: " +
                                       std::string(e.what()));
                break;
            }
        }

        if (!mod_entries.empty())
        {
            mod_entries.front()->get_download_button()->set_nav(NavDirection::Up,
                                                                refresh_button);
            mod_entries.back()->get_download_button()->set_nav(NavDirection::Down,
                                                               close_button);
        }

        queue_update();
    }

    void ModDownloadsPanel::process_event(const Event & /*e*/) {}

    // Creates an Rml host element for the marketplace modal container
    ElementModDownloads::ElementModDownloads(const Rml::String &tag)
        : Rml::Element(tag)
    {
        SetProperty("width", "100%");
        SetProperty("height", "100%");
        SetProperty("position", "absolute");
        SetProperty("top", "0px");
        SetProperty("left", "0px");
        SetProperty("z-index", "1000");
    }

    ElementModDownloads::~ElementModDownloads() {}

}
