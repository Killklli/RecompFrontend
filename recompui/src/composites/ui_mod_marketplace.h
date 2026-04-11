#ifndef RECOMPUI_MOD_MARKETPLACE_H
#define RECOMPUI_MOD_MARKETPLACE_H

#include "elements/ui_button.h"
#include "elements/ui_container.h"
#include "elements/ui_element.h"
#include "elements/ui_image.h"
#include "elements/ui_label.h"
#include "elements/ui_scroll_container.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace recompui
{

    std::string get_marketplace_url();
    void curl_global_initialize();
    std::string http_fetch_string(const std::string &url);
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

    struct MarketplaceMod
    {
        std::string name;
        std::string short_description;
        std::string file_url;
        std::string thumbnail_image;
        std::string version;
        std::string id;
        std::string game_id;
        std::vector<std::string> dependencies;
    };

    class ModMarketplaceEntry : public Element
    {
    public:
        MarketplaceMod mod_data;
        ModMarketplaceEntry(Element *parent, const MarketplaceMod &mod_data);
        virtual ~ModMarketplaceEntry();
        void
        set_download_callback(std::function<void(const MarketplaceMod &)> callback);
        void update_install_status(ModInstallStatus status);
        Button *get_download_button() { return download_button; }

    protected:
        std::string_view get_type_name() override { return "ModMarketplaceEntry"; }
        void process_event(const Event &e) override;

    private:
        Container *entry_container = nullptr;
        Image *thumbnail_image = nullptr;
        Label *name_label = nullptr;
        Label *description_label = nullptr;
        Button *download_button = nullptr;
        std::function<void(const MarketplaceMod &)> download_callback;
    };

    class ModDownloadsPanel : public Element
    {
    public:
        ModDownloadsPanel(Element *parent);
        virtual ~ModDownloadsPanel();
        void show();
        void hide();
        void fetch_marketplace_data();

    protected:
        std::string_view get_type_name() override { return "ModDownloadsPanel"; }
        void process_event(const Event &e) override;

    private:
        void load_marketplace_mods(const std::vector<MarketplaceMod> &mods);
        void download_mod(const MarketplaceMod &mod);
        std::string fetch_json_from_url(const std::string &url);
        std::vector<MarketplaceMod>
        parse_marketplace_json(const std::string &json_data);
        void download_file_from_url(const std::string &url,
                                    const std::string &output_path);
        bool is_mod_installed(const MarketplaceMod &mod);
        ModInstallStatus get_mod_install_status(const MarketplaceMod &mod);
        std::string get_installed_mod_version(const MarketplaceMod &mod);
        bool check_dependencies_satisfiable(const MarketplaceMod &mod) const;
        const MarketplaceMod *
        find_marketplace_mod_by_id(const std::string &mod_id) const;
        bool install_single_mod_file(const MarketplaceMod &mod,
                                     std::vector<std::string> &out_errors);
        void resolve_and_install_dependencies(
            const MarketplaceMod &mod, std::unordered_set<std::string> &visited_ids,
            std::vector<std::string> &out_warnings,
            std::vector<std::string> &out_errors,
            std::vector<std::string> &out_installed_deps);

        Container *main_container = nullptr;
        Container *content_panel = nullptr;
        Label *title_label = nullptr;
        Label *status_label = nullptr;
        ScrollContainer *mod_list_container = nullptr;
        Button *refresh_button = nullptr;
        Button *close_button = nullptr;
        std::vector<ModMarketplaceEntry *> mod_entries;
        std::vector<MarketplaceMod> fetched_mods;
        std::string fetch_error;
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