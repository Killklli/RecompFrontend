#include "../../../lib/N64ModernRuntime/N64Recomp/lib/tomlplusplus/vendor/json.hpp"
#include "./ui_mod_marketplace.h"
#include "librecomp/game.hpp"
#include "recompui/recompui.h"
#include "ui_utils.h"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

extern std::vector<recomp::GameEntry> supported_games;

namespace
{
    std::string to_lower_copy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool mod_name_matches_query(const recompui::MarketplaceMod &mod,
                                const std::string &query)
    {
        if (query.empty())
            return true;

        const std::string lower_name = to_lower_copy(mod.name);
        const std::string lower_query = to_lower_copy(query);
        return lower_name.find(lower_query) != std::string::npos;
    }
} // namespace

namespace recompui
{
    // Gets the URL for the marketplace so we can determine if we have to render the
    // button for the UI
    std::string get_marketplace_url()
    {
        return supported_games.empty() ? "" : supported_games[0].marketplace_url;
    }

    // Each Mod Entry on the UI
    ModMarketplaceEntry::ModMarketplaceEntry(ResourceId rid, Element *parent,
                                             const MarketplaceMod &mod_data)
                : Element(rid, parent, Events(EventType::Click)),
                    mod_data(mod_data)
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

        init_thumbnail_image();

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

    void ModMarketplaceEntry::process_event(const Event &e)
    {
        (void)e;
    }

    // Initializes the marketplace modal panel and child controls.
    ModDownloadsPanel::ModDownloadsPanel(ResourceId rid, Element *parent)
        : Element(rid, parent, Events(EventType::Update))
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
        header_container->set_margin_bottom(12.0f);

        title_label = context.create_element<Label>(
            header_container, "Mod Marketplace", LabelStyle::Large);
        title_label->set_color(Color{242, 242, 242, 255});

        refresh_button = context.create_element<Button>(header_container, "Refresh",
                                                        ButtonStyle::Secondary);
        refresh_button->add_pressed_callback([this]()
                                             { fetch_marketplace_data(); });

        // Filter row: search label + input on the left, sort button on the right
        Container *filter_container = context.create_element<Container>(
            content_panel, FlexDirection::Row, JustifyContent::SpaceBetween);
        filter_container->set_align_items(AlignItems::Center);
        filter_container->set_width(100.0f, Unit::Percent);

        // Left side: "Search:" label + text input — fixed width, does not grow
        Container *search_group = context.create_element<Container>(
            filter_container, FlexDirection::Row, JustifyContent::FlexStart);
        search_group->set_align_items(AlignItems::Center);
        search_group->set_gap(10.0f);
        search_group->set_flex(0.0f, 0.0f);
        search_group->set_width(280.0f, Unit::Px);

        Label *search_label = context.create_element<Label>(
            search_group, "Search:", LabelStyle::Small);
        search_label->set_color(Color{180, 180, 180, 255});

        search_input = context.create_element<TextInput>(search_group);
        search_input->set_flex(1.0f, 1.0f);
        search_input->set_min_width(120.0f);
        search_input->set_background_color(Color{40, 38, 50, 255});
        search_input->add_text_changed_callback(
            [this](const std::string &text)
            {
                name_search_query = text;
                refresh_marketplace_mods();
            });

        // Right side: sort toggle button
        sort_name_button =
            context.create_element<Button>(filter_container, "Name (A\u2192Z)",
                                           ButtonStyle::Secondary);
        sort_name_button->add_pressed_callback([this]()
                                               {
                                                   sort_name_ascending =
                                                       !sort_name_ascending;
                                                   sort_name_button->set_text(
                                                       sort_name_ascending
                                                           ? "Name (A\u2192Z)"
                                                           : "Name (Z\u2192A)");
                                                   refresh_marketplace_mods();
                                               });

        status_label = context.create_element<Label>(
            content_panel, "Loading marketplace data...", LabelStyle::Normal);
        status_label->set_color(Color{204, 204, 204, 255});
        status_label->set_text_align(TextAlign::Center);

        mod_list_container = context.create_element<ScrollContainer>(
            content_panel, ScrollDirection::Vertical);
        mod_list_container->set_flex(1.0f, 1.0f);
        mod_list_container->set_width(100.0f, Unit::Percent);
        mod_list_container->set_scroll_callback([this]()
                                                {
                                                    thumbnail_viewport_dirty = true;
                                                    thumbnail_viewport_retry_frames = 2;
                                                    queue_update();
                                                });

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
        refresh_button->set_nav(NavDirection::Up, sort_name_button);
        refresh_button->set_nav(NavDirection::Down, close_button);
        refresh_button->set_nav_none(NavDirection::Left);
        refresh_button->set_nav_none(NavDirection::Right);
        sort_name_button->set_nav(NavDirection::Up, search_input);
        sort_name_button->set_nav(NavDirection::Down, refresh_button);
        sort_name_button->set_nav(NavDirection::Left, search_input);
        sort_name_button->set_nav(NavDirection::Right, refresh_button);
        search_input->set_nav(NavDirection::Down, sort_name_button);
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
            load_marketplace_mods(fetched_mods);
        }
        catch (const std::exception &e)
        {
            status_label->set_text("Failed to load marketplace data: " +
                                   std::string(e.what()));
        }

        is_loading = false;
    }

    void ModDownloadsPanel::refresh_marketplace_mods()
    {
        if (is_loading)
            return;

        load_marketplace_mods(fetched_mods);
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
                try_get("thumbnail_url", mod.thumbnail_url);
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
            if ((mod.game_id.empty() || mod.game_id == current_game_id) &&
                mod_name_matches_query(mod, name_search_query))
                filtered_mods.push_back(mod);
        }

        std::sort(filtered_mods.begin(), filtered_mods.end(),
                  [this](const MarketplaceMod &a, const MarketplaceMod &b)
                  {
                      const std::string a_lower = to_lower_copy(a.name);
                      const std::string b_lower = to_lower_copy(b.name);
                      if (a_lower == b_lower)
                          return sort_name_ascending ? (a.name < b.name)
                                                     : (a.name > b.name);

                      return sort_name_ascending ? (a_lower < b_lower)
                                                 : (a_lower > b_lower);
                  });

        if (filtered_mods.empty())
        {
            if (name_search_query.empty())
            {
                status_label->set_text(
                    "No mods available for this game in marketplace");
            }
            else
            {
                status_label->set_text("No mods match \"" + name_search_query +
                                       "\"");
            }
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

        thumbnail_viewport_dirty = true;
        thumbnail_viewport_retry_frames = 3;
        queue_update();
    }

    void ModDownloadsPanel::process_event(const Event &e)
    {
        if (e.type != EventType::Update)
            return;

        bool has_pending_thumbnail_loads = false;
        for (ModMarketplaceEntry *entry : mod_entries)
        {
            if (entry != nullptr && entry->update_thumbnail_load(mod_list_container))
                has_pending_thumbnail_loads = true;
        }

        if (thumbnail_viewport_dirty)
        {
            thumbnail_viewport_dirty = false;

            constexpr size_t max_starts_per_scan = 12;
            size_t started_this_scan = 0;

            for (ModMarketplaceEntry *entry : mod_entries)
            {
                if (entry == nullptr)
                    continue;

                if (entry->start_thumbnail_load_if_visible(mod_list_container))
                {
                    started_this_scan++;
                    has_pending_thumbnail_loads = true;
                    if (started_this_scan >= max_starts_per_scan)
                        break;
                }
            }

            if (started_this_scan == 0 && thumbnail_viewport_retry_frames > 0)
            {
                thumbnail_viewport_retry_frames--;
                thumbnail_viewport_dirty = true;
                has_pending_thumbnail_loads = true;
            }
        }

        if (has_pending_thumbnail_loads)
            queue_update();
    }

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

    struct AsyncThumbnailLoadState
    {
        std::mutex mutex;
        bool completed = false;
        bool failed = false;
        std::vector<char> image_data;
    };

}

namespace
{
    // Moves the thumbnail loading work off the main ui so we can buffer ui updates to not kill out the UI as bad
    struct ThumbnailLoadTask
    {
        std::string url;
        std::shared_ptr<recompui::AsyncThumbnailLoadState> state;
    };

    // Main Handler for loading thumbnails, honestly I really don't like this code and would love a refactor here
    class MarketplaceThumbnailLoader
    {
    public:
        // Gets the instance
        static MarketplaceThumbnailLoader &instance()
        {
            static MarketplaceThumbnailLoader loader;
            return loader;
        }

        // Adds a thumbnail load task to the queue
        void enqueue(const std::string &url,
                     const std::shared_ptr<recompui::AsyncThumbnailLoadState> &state)
        {
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                tasks.emplace_back(ThumbnailLoadTask{url, state});
            }
            queue_cv.notify_one();
        }

    private:
        // Uses a pool of worker threads to process thumbnail load tasks in the background
        MarketplaceThumbnailLoader()
        {
            unsigned int worker_count = std::thread::hardware_concurrency();
            if (worker_count == 0)
                worker_count = 1;
            if (worker_count > 4)
                worker_count = 4;
            workers.reserve(worker_count);
            for (unsigned int i = 0; i < worker_count; ++i)
            {
                workers.emplace_back([this]()
                                     { worker_loop(); });
            }
        }

        ~MarketplaceThumbnailLoader()
        {
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                stopping = true;
            }
            queue_cv.notify_all();

            for (std::thread &worker : workers)
            {
                if (worker.joinable())
                    worker.join();
            }
        }

        // iterates through thumbnail loader tasks
        void worker_loop()
        {
            while (true)
            {
                ThumbnailLoadTask task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    queue_cv.wait(lock, [this]()
                                  { return stopping || !tasks.empty(); });

                    if (stopping && tasks.empty())
                        return;

                    task = std::move(tasks.front());
                    tasks.pop_front();
                }

                std::vector<char> image_data;
                bool failed = false;

                try
                {
                    image_data = recompui::http_fetch_bytes(task.url);
                }
                catch (...)
                {
                    failed = true;
                }

                if (!task.state)
                    continue;

                std::lock_guard<std::mutex> lock(task.state->mutex);
                task.state->failed = failed || image_data.empty();
                task.state->completed = true;
                task.state->image_data = std::move(image_data);
            }
        }

        std::mutex queue_mutex;
        std::condition_variable queue_cv;
        std::deque<ThumbnailLoadTask> tasks;
        std::vector<std::thread> workers;
        bool stopping = false;
    };
    // Sets the thumbnail source using the url data
    std::string make_marketplace_thumbnail_src(
        const recompui::MarketplaceMod &mod_data)
    {
        const std::string cache_key = !mod_data.id.empty()
                                          ? mod_data.id
                                          : (!mod_data.thumbnail_url.empty()
                                                 ? mod_data.thumbnail_url
                                                 : mod_data.name);
        return "marketplace_thumb_" +
               std::to_string(std::hash<std::string>{}(cache_key));
    }
} // namespace

namespace recompui
{
    void ModMarketplaceEntry::init_thumbnail_image()
    {
        // if we actually have a thumbnail, check if its base64 and if we need to
        // decode it or not.
        if (!mod_data.thumbnail_image.empty())
        {
            static constexpr const char *data_image_prefix = "data:image/";
            if (mod_data.thumbnail_image.compare(
                    0, std::char_traits<char>::length(data_image_prefix),
                    data_image_prefix) == 0)
            {
                thumbnail_src = make_marketplace_thumbnail_src(mod_data);
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
    }

    // Adds the url load to the queue
    void ModMarketplaceEntry::begin_thumbnail_url_load()
    {
        if (!thumbnail_load_started && !thumbnail_load_finished &&
            !mod_data.thumbnail_url.empty())
        {
            thumbnail_load_started = true;
            thumbnail_src = make_marketplace_thumbnail_src(mod_data);
            thumbnail_load_state = std::make_shared<AsyncThumbnailLoadState>();
            MarketplaceThumbnailLoader::instance().enqueue(mod_data.thumbnail_url,
                                                           thumbnail_load_state);
        }
    }

    // If the thumbnail is loaded, apply it to the UI
    bool ModMarketplaceEntry::try_apply_loaded_thumbnail()
    {
        if (!thumbnail_load_state)
            return true;

        std::vector<char> image_data;
        bool completed = false;
        bool failed = false;

        {
            std::lock_guard<std::mutex> lock(thumbnail_load_state->mutex);
            completed = thumbnail_load_state->completed;
            failed = thumbnail_load_state->failed;

            if (completed)
                image_data = std::move(thumbnail_load_state->image_data);
        }

        if (!completed)
            return false;

        thumbnail_load_state.reset();
        thumbnail_load_finished = true;

        if (!failed && !image_data.empty())
        {
            // recompui::queue_image_from_bytes_file(thumbnail_src, image_data);
            // thumbnail_image->set_src(thumbnail_src);
            thumbnail_image->queue_update();
        }

        return true;
    }

    // Checks if we need to apply a loaded thumbnail to the UI
    bool ModMarketplaceEntry::process_thumbnail_load()
    {
        if (!thumbnail_load_state)
            return false;

        return !try_apply_loaded_thumbnail();
    }

    // Gets the list of mods that are visible in the current scrollbar
    bool ModMarketplaceEntry::is_visible_in_viewport(ScrollContainer *viewport) const
    {
        if (viewport == nullptr)
            return false;

        const float viewport_top = viewport->get_absolute_top();
        const float viewport_bottom = viewport_top + viewport->get_client_height();
        const float entry_top = entry_container->get_absolute_top();
        const float entry_bottom = entry_top + entry_container->get_client_height();

        return entry_bottom >= viewport_top && entry_top <= viewport_bottom;
    }

    // Status check to validate if everythings loaded
    bool ModMarketplaceEntry::has_thumbnail_work_remaining() const
    {
        return !mod_data.thumbnail_url.empty() && !thumbnail_load_finished;
    }

    // Checks if we can update the load based on the scroll position
    bool ModMarketplaceEntry::update_thumbnail_load(ScrollContainer *viewport)
    {
        if (thumbnail_load_state)
            return !try_apply_loaded_thumbnail();

        return false;
    }

    // Starts the thumbnail load if we are visible in the viewport and we haven't started loading yet
    bool ModMarketplaceEntry::start_thumbnail_load_if_visible(
        ScrollContainer *viewport)
    {
        if (!thumbnail_load_started && has_thumbnail_work_remaining() &&
            is_visible_in_viewport(viewport))
        {
            begin_thumbnail_url_load();
            return true;
        }

        return false;
    }

}
