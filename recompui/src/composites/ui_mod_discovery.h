#ifndef RECOMPUI_MOD_DISCOVERY_H
#define RECOMPUI_MOD_DISCOVERY_H

#include "elements/ui_button.h"
#include "elements/ui_container.h"
#include "elements/ui_element.h"
#include "elements/ui_image.h"
#include "elements/ui_label.h"
#include "elements/ui_scroll_container.h"
#include "elements/ui_text_input.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace recompui
{

    std::string get_discovery_url();
    void curl_global_initialize();
    std::string http_fetch_string(const std::string &url);
    std::vector<char> http_fetch_bytes(const std::string &url);
    void http_download_to_file(const std::string &url,
                               const std::string &output_path);
    std::vector<char> decode_base64(const std::string &encoded);
    std::pair<std::string, std::string>
    parse_dep_string(const std::string &dep_str);
    int compare_versions(const std::string &version1,
                         const std::string &version2);

    enum class ModInstallStatus
    {
        NotInstalled,
        Installed,
        UpdateAvailable,
        DowngradeAvailable,
        MissingDependencies
    };

    struct DiscoveryMod
    {
        std::string name;
        std::string short_description;
        std::string file_url;
        std::string thumbnail_image;
        std::string thumbnail_url;
        std::string version;
        std::string id;
        std::string game_id;
        std::vector<std::string> dependencies;
    };

    struct AsyncThumbnailLoadState;

    class ModDiscoveryEntry : public Element
    {
    public:
        DiscoveryMod mod_data;
        ModDiscoveryEntry(ResourceId rid, Element *parent, const DiscoveryMod &mod_data);
        virtual ~ModDiscoveryEntry();
        void
        set_download_callback(std::function<void(const DiscoveryMod &)> callback);
        bool update_thumbnail_load(ScrollContainer *viewport);
        bool start_thumbnail_load_if_visible(ScrollContainer *viewport);
        void update_install_status(ModInstallStatus status);
        bool process_thumbnail_load();
        Button *get_download_button() { return download_button; }

    protected:
        std::string_view get_type_name() override { return "ModDiscoveryEntry"; }
        void process_event(const Event &e) override;

    private:
        void init_thumbnail_image();
        void begin_thumbnail_url_load();
        bool try_apply_loaded_thumbnail();
        bool is_visible_in_viewport(ScrollContainer *viewport) const;
        bool has_thumbnail_work_remaining() const;

        Container *entry_container = nullptr;
        Image *thumbnail_image = nullptr;
        Label *name_label = nullptr;
        Label *description_label = nullptr;
        Button *download_button = nullptr;
        std::shared_ptr<AsyncThumbnailLoadState> thumbnail_load_state;
        bool thumbnail_load_started = false;
        bool thumbnail_load_finished = false;
        std::string thumbnail_src;
        std::function<void(const DiscoveryMod &)> download_callback;
    };

    class ModDownloadsPanel : public Element
    {
    public:
        ModDownloadsPanel(ResourceId rid, Element *parent);
        virtual ~ModDownloadsPanel();
        void show();
        void hide();
        void fetch_discovery_data();

    protected:
        std::string_view get_type_name() override { return "ModDownloadsPanel"; }
        void process_event(const Event &e) override;

    private:
        void load_discovery_mods(const std::vector<DiscoveryMod> &mods);
        void refresh_discovery_mods();
        void download_mod(const DiscoveryMod &mod);
        std::string fetch_json_from_url(const std::string &url);
        std::vector<DiscoveryMod>
        parse_discovery_json(const std::string &json_data);
        void download_file_from_url(const std::string &url,
                                    const std::string &output_path);
        bool is_mod_installed(const DiscoveryMod &mod);
        ModInstallStatus get_mod_install_status(const DiscoveryMod &mod);
        std::string get_installed_mod_version(const DiscoveryMod &mod);
        bool check_dependencies_satisfiable(const DiscoveryMod &mod) const;
        const DiscoveryMod *
        find_discovery_mod_by_id(const std::string &mod_id) const;
        bool install_single_mod_file(const DiscoveryMod &mod,
                                     std::vector<std::string> &out_errors);
        void resolve_and_install_dependencies(
            const DiscoveryMod &mod, std::unordered_set<std::string> &visited_ids,
            std::vector<std::string> &out_warnings,
            std::vector<std::string> &out_errors,
            std::vector<std::string> &out_installed_deps);

        Container *main_container = nullptr;
        Container *content_panel = nullptr;
        Label *title_label = nullptr;
        Label *status_label = nullptr;
        TextInput *search_input = nullptr;
        Button *sort_name_button = nullptr;
        ScrollContainer *mod_list_container = nullptr;
        Button *refresh_button = nullptr;
        Button *close_button = nullptr;
        std::vector<ModDiscoveryEntry *> mod_entries;
        std::vector<DiscoveryMod> fetched_mods;
        std::string name_search_query;
        bool sort_name_ascending = true;
        std::string fetch_error;
        bool thumbnail_viewport_dirty = false;
        int thumbnail_viewport_retry_frames = 0;
        bool is_visible = false;
        bool is_loading = false;
        bool fetch_completed = false;
    };
    class ElementModDownloads : public Rml::Element
    {
    public:
        ElementModDownloads(const Rml::String &tag);
        virtual ~ElementModDownloads();
    };

}

#endif