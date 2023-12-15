#include <application.h>
#include <UI.h>
#include <getopt.h>

#define ENABLE_MULTI_VIEWPORT   1

static std::string ini_file = "test_blueprint.ini";
static std::string bluepoint_file = "test_bp.json";

static int OnBluePrintChange(int type, std::string name, void* handle)
{
    int ret = BluePrint::BP_CBR_Nothing;
    if (/*type == BluePrint::BP_CB_Link ||
        type == BluePrint::BP_CB_Unlink ||
        type == BluePrint::BP_CB_NODE_DELETED ||
        type == BluePrint::BP_CB_NODE_APPEND ||*/
        type == BluePrint::BP_CB_NODE_INSERT)
    {
        // need update
        ret = BluePrint::BP_CBR_AutoLink;
    }
    else if (type == BluePrint::BP_CB_PARAM_CHANGED ||
            type == BluePrint::BP_CB_SETTING_CHANGED)
    {
        return BluePrint::BP_CBR_RunAgain;
    }
    return ret;
}

static void BlueprintTest_SetupContext(ImGuiContext* ctx, bool in_splash)
{
    if (!ctx)
        return;
#ifdef USE_BOOKMARK
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        if (dialog) dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

static void BlueprintTest_Initialize(void** handle)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
    BluePrint::BluePrintUI * UI = new BluePrint::BluePrintUI();
    UI->Initialize(bluepoint_file.c_str());
    BluePrint::BluePrintCallbackFunctions callbacks;
    callbacks.BluePrintOnChanged = OnBluePrintChange;
    UI->SetCallbacks(callbacks, nullptr);
    *handle = UI;
#ifdef USE_THUMBNAILS
    ImGuiFileDialog::Instance()->SetCreateThumbnailCallback([](IGFD_Thumbnail_Info *vThumbnail_Info) -> void
    {
        if (vThumbnail_Info && 
            vThumbnail_Info->isReadyToUpload && 
            vThumbnail_Info->textureFileDatas)
        {
            auto texture = ImGui::ImCreateTexture(vThumbnail_Info->textureFileDatas, vThumbnail_Info->textureWidth, vThumbnail_Info->textureHeight);
            vThumbnail_Info->textureID = (void*)texture;
            delete[] vThumbnail_Info->textureFileDatas;
            vThumbnail_Info->textureFileDatas = nullptr;

            vThumbnail_Info->isReadyToUpload = false;
            vThumbnail_Info->isReadyToDisplay = true;
        }
    });
    ImGuiFileDialog::Instance()->SetDestroyThumbnailCallback([](IGFD_Thumbnail_Info* vThumbnail_Info)
    {
        if (vThumbnail_Info && vThumbnail_Info->textureID)
        {
            ImTextureID texID = (ImTextureID)vThumbnail_Info->textureID;
            ImGui::ImDestroyTexture(texID);
        }
    });
#endif
}

static void BlueprintTest_Finalize(void** handle)
{
    BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)*handle;
    if (!UI)
        return;
    UI->Finalize();
    delete UI;
}

static bool BlueprintTest_Frame(void * handle, bool app_will_quit)
{
    BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)handle;
    if (!UI)
        return true;
#ifdef USE_THUMBNAILS
	ImGuiFileDialog::Instance()->ManageGPUThumbnails();
#endif
    return UI->Frame() || app_will_quit;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    // param commandline args
    std::vector<std::string> plugin_path;
    static struct option long_options[] = {
        { "plugin_dir", required_argument, NULL, 'p' },
        { 0, 0, 0, 0 }
    };
    if (property.argc > 1 && property.argv)
    {
        int o = -1;
        int option_index = 0;
        while ((o = getopt_long(property.argc, property.argv, "p:", long_options, &option_index)) != -1)
        {
            if (o == -1)
                break;
            switch (o)
            {
                case 'p': plugin_path.push_back(std::string(optarg)); break;
                default: break;
            }
        }
    }

    property.name = "BlueprintSDK Test";
    property.docking = false;
    property.resizable = false;
    property.full_size = true;
    property.auto_merge = false;
    property.font_scale = 2.0f;
    property.application.Application_SetupContext = BlueprintTest_SetupContext;
    property.application.Application_Initialize = BlueprintTest_Initialize;
    property.application.Application_Finalize = BlueprintTest_Finalize;
    property.application.Application_Frame = BlueprintTest_Frame;
    int index = 0;
    float percentage = 0;
    std::string message;
    BluePrint::BluePrintUI::LoadPlugins(plugin_path, index, message, percentage, 0);
}