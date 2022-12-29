#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <CopyTo_vulkan.h>

typedef enum Move_Type : int32_t
{
    MOVE_RIGHT = 0,
    MOVE_LEFT,
    MOVE_BOTTOM,
    MOVE_TOP,
} Move_Type;

namespace BluePrint
{
struct MoveFusionNode final : Node
{
    BP_NODE_WITH_NAME(MoveFusionNode, "Move Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    MoveFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Move Transform"; }

    ~MoveFusionNode()
    {
        if (m_copy) { delete m_copy; m_copy = nullptr; }
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
    }

    void OnStop(Context& context) override
    {
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        int x1 = 0, y1 = 0;
        int x2 = 0, y2 = 0;
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float percentage = context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            switch (m_move_type)
            {
                case MOVE_RIGHT :
                    x1 = - percentage * mat_first.w;
                    x2 = (1.0 - percentage) * mat_first.w;
                break;
                case MOVE_LEFT:
                    x1 = percentage * mat_first.w;
                    x2 = - (1.0 - percentage) * mat_first.w;
                break;
                case MOVE_BOTTOM:
                    y1 = - percentage * mat_first.h;
                    y2 = (1.0 - percentage) * mat_first.h;
                break;
                case MOVE_TOP:
                    y1 = percentage * mat_first.h;
                    y2 = - (1.0 - percentage) * mat_first.h;
                break;
                default: break;
            }
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_copy || m_device != gpu)
            {
                if (m_copy) { delete m_copy; m_copy = nullptr; }
                m_copy = new ImGui::CopyTo_vulkan(gpu);
            }
            if (!m_copy)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            double node_time = 0;
            node_time += m_copy->copyTo(mat_first, im_RGB, x1, y1);
            node_time += m_copy->copyTo(mat_second, im_RGB, x2, y2);
            m_NodeTimeMs = node_time;
            im_RGB.time_stamp = mat_first.time_stamp;
            im_RGB.rate = mat_first.rate;
            im_RGB.flags = mat_first.flags;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        int type = m_move_type;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::RadioButton("Right In", &type, MOVE_RIGHT);
        ImGui::RadioButton("Left In", &type, MOVE_LEFT);
        ImGui::RadioButton("Bottom In", &type, MOVE_BOTTOM);
        ImGui::RadioButton("Top In", &type,MOVE_TOP);
        if (type != m_move_type) { m_move_type = type; changed = true; }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("move_type"))
        { 
            auto& val = value["move_type"];
            if (val.is_number())
                m_move_type = (Move_Type)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["move_type"] = imgui_json::number(m_move_type);
    }

    void load_logo() const
    {
        int width = 0, height = 0, component = 0;
        if (auto data = stbi_load_from_memory((stbi_uc const *)logo_data, logo_size, &width, &height, &component, 4))
        {
            m_logo = ImGui::ImCreateTexture(data, width, height);
        }
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        // if show icon then we using u8"\ue883"
        if (!m_logo)
        {
            load_logo();
        }
        if (m_logo)
        {
            int logo_col = (m_logo_index / 4) % 4;
            int logo_row = (m_logo_index / 4) / 4;
            float logo_start_x = logo_col * 0.25;
            float logo_start_y = logo_row * 0.25;
            ImGui::Image(m_logo, size, ImVec2(logo_start_x, logo_start_y),  ImVec2(logo_start_x + 0.25f, logo_start_y + 0.25f));
            m_logo_index++; if (m_logo_index >= 64) m_logo_index = 0;
        }
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatInFirst   = { this, "In 1" };
    MatPin    m_MatInSecond   = { this, "In 2" };
    FloatPin  m_Pos = { this, "Pos" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    int m_move_type   {MOVE_RIGHT};
    ImGui::CopyTo_vulkan * m_copy   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4612;
    const unsigned int logo_data[4612/4] =
{
    0xe0ffd8ff, 0x464a1000, 0x01004649, 0x01000001, 0x00000100, 0x8400dbff, 0x07070a00, 0x0a060708, 0x0b080808, 0x0e0b0a0a, 0x0d0e1018, 0x151d0e0d, 
    0x23181116, 0x2224251f, 0x2621221f, 0x262f372b, 0x21293429, 0x31413022, 0x3e3b3934, 0x2e253e3e, 0x3c434944, 0x3e3d3748, 0x0b0a013b, 0x0e0d0e0b, 
    0x1c10101c, 0x2822283b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 
    0x3b3b3b3b, 0x3b3b3b3b, 0xc0ff3b3b, 0x00081100, 0x03000190, 0x02002201, 0x11030111, 0x01c4ff01, 0x010000a2, 0x01010105, 0x00010101, 0x00000000, 
    0x01000000, 0x05040302, 0x09080706, 0x00100b0a, 0x03030102, 0x05030402, 0x00040405, 0x017d0100, 0x04000302, 0x21120511, 0x13064131, 0x22076151, 
    0x81321471, 0x2308a191, 0x15c1b142, 0x24f0d152, 0x82726233, 0x17160a09, 0x251a1918, 0x29282726, 0x3635342a, 0x3a393837, 0x46454443, 0x4a494847, 
    0x56555453, 0x5a595857, 0x66656463, 0x6a696867, 0x76757473, 0x7a797877, 0x86858483, 0x8a898887, 0x95949392, 0x99989796, 0xa4a3a29a, 0xa8a7a6a5, 
    0xb3b2aaa9, 0xb7b6b5b4, 0xc2bab9b8, 0xc6c5c4c3, 0xcac9c8c7, 0xd5d4d3d2, 0xd9d8d7d6, 0xe3e2e1da, 0xe7e6e5e4, 0xf1eae9e8, 0xf5f4f3f2, 0xf9f8f7f6, 
    0x030001fa, 0x01010101, 0x01010101, 0x00000001, 0x01000000, 0x05040302, 0x09080706, 0x00110b0a, 0x04020102, 0x07040304, 0x00040405, 0x00770201, 
    0x11030201, 0x31210504, 0x51411206, 0x13716107, 0x08813222, 0xa1914214, 0x2309c1b1, 0x15f05233, 0x0ad17262, 0xe1342416, 0x1817f125, 0x27261a19, 
    0x352a2928, 0x39383736, 0x4544433a, 0x49484746, 0x5554534a, 0x59585756, 0x6564635a, 0x69686766, 0x7574736a, 0x79787776, 0x8483827a, 0x88878685, 
    0x93928a89, 0x97969594, 0xa29a9998, 0xa6a5a4a3, 0xaaa9a8a7, 0xb5b4b3b2, 0xb9b8b7b6, 0xc4c3c2ba, 0xc8c7c6c5, 0xd3d2cac9, 0xd7d6d5d4, 0xe2dad9d8, 
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xb2a22820, 0xc9a9493c, 0xa0c1e9a6, 
    0xc0c8937d, 0x1d38c021, 0xf3fc074f, 0xb02bbb49, 0x7faee835, 0xddb57ac3, 0x9716d7e4, 0xf9e399ed, 0xc7295486, 0xd3073042, 0x9482aef3, 0x80aea464, 
    0xa08aa228, 0x80a2280a, 0x2eae280a, 0x92bac4f7, 0x6bc933ea, 0x90226d2e, 0x418add2e, 0x9c675c1f, 0x474d891a, 0x14a59d71, 0xe15965c8, 0x5de55049, 
    0x69ec0343, 0xb70a62f5, 0xffa8fe6b, 0x2a55bd00, 0xa3faafdd, 0x6273f5fe, 0x536786bf, 0x45512ae1, 0x1468d215, 0x35894756, 0x38dd3439, 0xf2641f18, 
    0x0e700830, 0xffc15307, 0x6fd53c00, 0x77d7ea0d, 0x5f5a5c93, 0xe68f67b6, 0x1da75019, 0x4f1fc008, 0x739da3ce, 0x28e88c72, 0x0eb18aa2, 0x7f6afd8f, 
    0x50fd2abc, 0x7f6afd8f, 0x79fd2abc, 0x1889cfd8, 0xa2cedc55, 0x37d12b8a, 0xbd8b2b0a, 0xbaa42ef1, 0xcb5af28c, 0x0ba4489b, 0x479062b7, 0x06e719d7, 
    0x596518bb, 0xe55049e1, 0xec03435d, 0x2d35236a, 0x288a3e86, 0xd76e11ab, 0x00ff51fd, 0x6da8a67a, 0xf71fd57f, 0xebf16aaa, 0x9c1100ff, 0x9d99f8b3, 
    0xb0571445, 0x7a710575, 0x9e0ca3fe, 0xac4c8823, 0x18105af0, 0x9f7a522f, 0xed0a3fe8, 0xd14e3b2b, 0x66aed360, 0x966549b9, 0x48f3be59, 0x414fae47, 
    0x5a2ece59, 0xfe96a321, 0x00ffb7d8, 0x1b359984, 0x9c389275, 0x0e7c2881, 0xba9d1ebc, 0xd5baee1a, 0xb748571d, 0xe748e2d5, 0x908d5d67, 0x39fd8cc9, 
    0xe44570ab, 0x676f91c0, 0x3777a1d8, 0x17218a53, 0xe803ee16, 0x41d08aa2, 0x06501445, 0xf67bb97e, 0x8779260d, 0x7d6a23df, 0x5bc7154f, 0x6831166a, 
    0x14c9d833, 0x765b79a6, 0x1d0106f0, 0x9c00ff3b, 0xe9aa65d7, 0xc7f16a11, 0x884ad31c, 0x724c3887, 0xec2af27d, 0x2431ac51, 0xa1a83048, 0x9415f640, 
    0x9821e5a0, 0xfbbd117e, 0x76f69746, 0x63bbbd6c, 0x479eee9f, 0x7aabfcf5, 0x4b346cb3, 0x5bf23a7d, 0xf7932498, 0x1b67ccb9, 0x8ae33947, 0x6a82abd2, 
    0xb70a6236, 0xffa8fe6b, 0x2a55bd00, 0xa3faafdd, 0x8a61f5fe, 0x4f9d19fe, 0x1445a984, 0xc5a14957, 0x328cfaeb, 0x32218e78, 0x4068c1b3, 0xea49bd60, 
    0x2afca07f, 0x7f8bed1f, 0x519349f8, 0x892359b7, 0xc08712c8, 0xdbe9c1eb, 0x4ea7aea1, 0xaed320d1, 0x6449b966, 0xf3be5996, 0x4fae4748, 0x74d54f41, 
    0x245e7d8b, 0xd875768e, 0xcf980cd9, 0xc2c19ad3, 0xbddc514f, 0x11648ad6, 0x5b243079, 0x5d28f6d9, 0x9fe2d4cd, 0x7f74885b, 0xe1fd53eb, 0x7f84ea57, 
    0xe1fd53eb, 0xc6ceeb57, 0xaec2487c, 0xeb6775e6, 0xd260bf97, 0xf27d9867, 0xf1d4a736, 0x5543155a, 0xe3d522d2, 0x95a6398e, 0x98700e11, 0x57e4fbe4, 
    0x0d6daf7c, 0x50db36ce, 0x9e418bb1, 0x33a548c6, 0x80b7dbca, 0xdfe90830, 0x3fbae6fc, 0xa3fdde08, 0x363bfb4b, 0xcfb1dd5e, 0xfa234ff7, 0x14b955fe, 
    0x12490c6b, 0x50282a0c, 0xd867853d, 0x75fa9668, 0x4930b7e4, 0x9873ef27, 0x738e36ce, 0x609c15c7, 0xcd1dd3e2, 0xb6a2282a, 0xfdd76e11, 0x7a00ff51, 
    0x7f6da8a6, 0xaaf71fd5, 0xffebf16a, 0xb39c1100, 0x459d99f8, 0x50f75a15, 0x59a2d3b5, 0xd5f2e56e, 0x493b689b, 0xd82bfcc9, 0x6651c76e, 0xf2d2868a, 
    0x3d01fade, 0x64649eb4, 0xfe080e90, 0x14003575, 0x70ccd3e8, 0xb569c6b9, 0xf5fabf25, 0x45957ffc, 0x421bd446, 0xb25f227a, 0x51fab77f, 0xfff64ff6, 
    0x45b14a00, 0xab599f79, 0x92f6e7dc, 0xa8a2a833, 0xb269ae47, 0x2e8d7dde, 0x4d719f41, 0xf7a81bbb, 0x6dd52bc6, 0xfd92ce2d, 0x024c5114, 0xccdf8ea6, 
    0x9ff1db40, 0xdbae866a, 0x5fa800ff, 0x3ed7f9c7, 0x2f847226, 0x5ada2612, 0xfd93fd11, 0x8aa5d2bf, 0xe74a293f, 0x459fe63c, 0x39add379, 0x399849ab, 
    0x459db937, 0x2dd4bd56, 0x5b96e874, 0x66b57cb9, 0x72d20eda, 0xa57d0a7f, 0x02f4bde5, 0xc83c697b, 0x111c20c9, 0xbad8ebfc, 0x6aa28ebd, 0x0e98a228, 
    0x0bc73c8d, 0xfba7669c, 0xa57ffb27, 0xbdfe6f45, 0x57e51f7f, 0x5613876b, 0xcca29d70, 0x849e2667, 0x83b56b0b, 0xd273e86f, 0xe28aa2a7, 0xe24f499d, 
    0xee4d4e66, 0x23545167, 0x6fd934d7, 0x2097c63e, 0xdda6b8cf, 0xe37bd48d, 0xa4bd7e15, 0x0aebd8d3, 0x1398a228, 0x20e66f47, 0xb5cff86d, 0xed9fec3f, 
    0xbf2595fe, 0x7ffc85fa, 0x5d5e499d, 0x3645454c, 0xe4dc3993, 0xf2a3c898, 0xce73ae94, 0xcd55f469, 0xd5be493f, 0x0fca824c, 0xdf8dc734, 0xd333ae3a, 
    0xce951ea9, 0xdcbb93db, 0xab305b8d, 0xabf3d695, 0x4cdb8af8, 0xb9b8fb53, 0xfd991f93, 0x51d7f931, 0x3c51f92b, 0x49b51384, 0xf8938cc2, 0xe8a61d57, 
    0xc595fa97, 0x1397ecc5, 0x79ce4858, 0xf21c4328, 0xd72b8e7b, 0x8e6477a9, 0xbcf05bc4, 0xbc51cba6, 0x2ac8a4d2, 0xf13d97c5, 0x5d31fdc1, 0xe9b37145, 
    0xaf36fa57, 0x4d92766d, 0x7197937d, 0x081d2719, 0xd9d59e3c, 0x5a9f9103, 0x60d6de29, 0xffb7a4c2, 0x8fbf5e00, 0x4beaa8f2, 0xf8ebf57f, 0x752a00ff, 
    0x137a097e, 0x8a76992d, 0xe414af28, 0xc14bf530, 0x73cfa661, 0x5c7e51c6, 0xad1ff4fa, 0x9a4f2771, 0xd6b749db, 0x3e432601, 0xfcb1837e, 0x7febfcc1, 
    0x76756bc5, 0xe16dac6d, 0x739bd595, 0x3b2aa1b2, 0xa1ce9f0c, 0x7f77c297, 0x5e0d3164, 0x965f4547, 0xc7698722, 0xd6abf741, 0x2d7a729a, 0x5674d48e, 
    0x69770997, 0xd97dc215, 0xaba93014, 0x728fc20b, 0x736b0f96, 0x9b98b10c, 0xc808bd29, 0xd67c5f3f, 0x75176ded, 0xb75d0571, 0xbf5000ff, 0x55aaf38f, 
    0x42fddf76, 0xb9ce3ffe, 0xf5027fb1, 0x25b1ab33, 0x615e5114, 0xde9a70ce, 0x5b117f75, 0x777f8a69, 0xf3633217, 0x3a3fa63f, 0x36e58577, 0x95e68d5a, 
    0x2c564126, 0x0f8eefb9, 0xa6a98ae9, 0x95fa97e8, 0x97ecc5c5, 0xce485813, 0x1c432879, 0x298e7bf2, 0xf4efd267, 0xedda5e6d, 0x27fb9a24, 0x4e32e22e, 
    0x3d79103a, 0xfcd6d5ab, 0xca8e3bf6, 0xcfc8018a, 0x49d215ad, 0xf5fabf25, 0x5d957ffc, 0x00ff56aa, 0xfef1d7eb, 0xccbc7655, 0x0ebdc65f, 0x1485bb7a, 
    0x67865c51, 0x6b3e9d96, 0x58df266d, 0xf90c9904, 0xf3c70efa, 0xeeaef307, 0xed2ee1d2, 0xbb4fb822, 0xce158622, 0xbf3be1cb, 0xaf8618b2, 0xcbafa223, 
    0xe3b44311, 0x6fd5fba0, 0x96728fc2, 0x0c736b0f, 0x299b98b1, 0x3fc808bd, 0xafd77c5f, 0x34bb384d, 0x8a76b376, 0x5dc4ad28, 0x5000ffb7, 0xa9f38fbf, 
    0xf57f3b2a, 0x00fff80b, 0x5abc923a, 0x4ebd1cbf, 0xbbc26e49, 0x00ff870f, 0xddbf6feb, 0xb89afd4f, 0x093cebfa, 0xde120c71, 0x7924d3f9, 0x03f6c654, 
    0x43697d3d, 0xc49f2ae2, 0x288a0a8e, 0x0ae96caf, 0x7fada928, 0x6af79fd6, 0xb9482e27, 0x902bbb09, 0x00ff96d4, 0xfef1d7eb, 0xe18a7655, 0x2d9a2f96, 
    0xbad4c558, 0x288a82b5, 0x3a2323ae, 0x78af288a, 0xab280aec, 0xaa00fff1, 0x6145f74f, 0x49b2af5a, 0x5c4ae4da, 0x6fbb0aa5, 0x1f7fa1fe, 0x5c5152e7, 
    0xd21eb135, 0xa794b136, 0x5114accc, 0x99197345, 0x7b4551d4, 0x535160c7, 0xad00ff5a, 0xbad5ee3f, 0xf688abe5, 0x9cb1e572, 0x52eccaa7, 0x5e00ffb7, 
    0xabf28fbf, 0x355c51b4, 0x5e49fbaa, 0x339732c6, 0x154551b8, 0x14752689, 0xd8f15e51, 0xfff85514, 0xfb27d500, 0x2f5c9da2, 0x5f6e6719, 0xf9dac7c4, 
    0x00ffdb11, 0xf9c75fa8, 0x5c5194d4, 0x9b349732, 0xe1ee6e32, 0x88541445, 0x411d74e7, 0x392e2db5, 0xb9ca20a4, 0x1da90f59, 0xefb846eb, 0x5aa4de08, 
    0xcc8af694, 0x182d9b22, 0xd71fb96e, 0x1abbca1f, 0x9acbe9f6, 0x9a0a6b27, 0xff69fdd7, 0xa9a17600, 0x9fd67fad, 0xff2b6af7, 0x3f910d00, 0xa2e89685, 
    0x52cef18a, 0x3e4984c5, 0x83146da3, 0x2b2a4972, 0x9120e30c, 0x4df5ec9a, 0x43bac237, 0x4e76b7c4, 0x2aeddc3c, 0xfd637aee, 0xb4e37aaf, 0x590bf9bf, 
    0x74d700ff, 0x85d000ff, 0xd623de77, 0xb625d2a1, 0x74e96912, 0x9654dc64, 0xd331ae00, 0xc25aeb83, 0x429b6dd6, 0xc9b3addc, 0x75daaaa8, 0xf069dfda, 
    0xebfba9dc, 0xfe7bec93, 0x94bd66b5, 0x7e156cee, 0xfe49f53f, 0x7e15aae8, 0xfe49f53f, 0x672caee8, 0xecaa8cc2, 0xaf288a3a, 0x7cebc038, 0xbae9a227, 
    0xf73c9b96, 0xa46bdeb6, 0xc3de41a5, 0x22f60003, 0x57fae9ad, 0x95c4f585, 0x8de5ad6c, 0x7649c6a3, 0x52a76704, 0x817f6a47, 0x936b344e, 0xc763e28c, 
    0xda5404fc, 0xb10ebd0e, 0xe991b62c, 0x124ad586, 0xf3e3265e, 0xf1d66000, 0x8ae9c84a, 0x2dcf958d, 0xd72b8aa2, 0xfdd79a2c, 0x7600ff69, 0x5f4bd5ad, 
    0xdafda7f5, 0x2b565eb7, 0x4f3d87f8, 0x84c52a88, 0x6da33e49, 0x49728314, 0xe30c2b2a, 0xaf9a9120, 0xf9bfb456, 0x00ff590b, 0x00ff74d7, 0xad7385d0, 
    0x8e9d5bc8, 0x57f8a6a9, 0xee967848, 0x9d9bc7c9, 0x4ccf5da5, 0x59efb57f, 0xfdf02b9e, 0x0d6b998e, 0x12e496dd, 0x6219c33e, 0x9e91137b, 0xe241577b, 
    0x221d6a3d, 0x9e26615b, 0xc54d4697, 0xe30a6049, 0xb53e381d, 0xb116c54b, 0x4d1f5e6a, 0xcbb08451, 0xdc082b1a, 0x8223019e, 0x25696b3d, 0x25898ead, 
    0x74e69166, 0xe57a4551, 0x00ffe397, 0x8aee9f54, 0xf53f3675, 0xa7e8fe49, 0x892f8757, 0xae708f9c, 0x2e7ac2b7, 0xb369a99b, 0xe66d7bcf, 0x1d544aba, 
    0x0f3030ec, 0xae922b62, 0x1aa7c0ef, 0x71c6c935, 0x02fee331, 0x975dc129, 0x462d274d, 0x8557fa69, 0x6c95c4f5, 0xa38de5ad, 0x047649c6, 0x4752a767, 
    0xe06ee36a, 0xdb93d716, 0x9122dc86, 0x0eae7793, 0x83de5d33, 0x4bac43af, 0x617aa42d, 0x978452b5, 0xc0fcb889, 0xebc73518, 0xbae911d6, 0x39d1f6c4, 
    0xdc085264, 0x79464672, 0xba569aaa, 0xa672692a, 0x44e72e8e, 0x23d5b8b7, 0x23c97cd4, 0x31f73199, 0x3f5e8f83, 0xa4a2d20a, 0x3f371f86, 0x5eed3136, 
    0x9c609cb4, 0x5776ab99, 0xaf357564, 0xedfed3fa, 0xed9fec3f, 0xa0f894fe, 0xecb698f2, 0xcd958ef1, 0x4139bd56, 0x38e5cca4, 0x14454bb4, 0x49609a57, 
    0xb9b6316d, 0x9800758a, 0xa107389c, 0xdf74cdc1, 0xff5e9ff0, 0x1f9ccf00, 0x8a56ae99, 0x29b626a5, 0x2d8cad49, 0x4b8f4d33, 0x4bf136b6, 0xc3e2a623, 
    0x85fc387e, 0xfdaca25c, 0xd2bffd93, 0x855489bd, 0xb974263b, 0xaf5ab925, 0x3fa9fec7, 0xd90f15dd, 0x2afddb3f, 0x2f541b75, 0x6271c5a0, 0x7269c26a, 
    0x4d921ab3, 0x51142d68, 0xd646465c, 0x8d5be28b, 0x86e0d916, 0xfb0ee408, 0xd07372c9, 0xa457e90f, 0xc8a0e6f1, 0x8c405b42, 0x1f27377a, 0x55f472ad, 
    0x9c523429, 0xeaccb592, 0xf361482a, 0x1e63f373, 0x3fd92fd5, 0xd62bfddb, 0x76717a95, 0x8a73e86c, 0x7f2d6376, 0x6af79fd6, 0x1e1445dd, 0x9edd1653, 
    0xafa5d231, 0xce38113b, 0xda848977, 0x2da9406f, 0x31d736a6, 0x1313a04e, 0x38f40087, 0xace8a839, 0xf8af3a08, 0xe77faf4f, 0xd6cc0fce, 0x4be2af66, 
    0x143162ed, 0x1c8423bb, 0x098e71f9, 0x8aacf5f7, 0x299b9c2a, 0x67764fce, 0xc97e5651, 0x47e9dffe, 0xfddb3fd9, 0xcdfad52b, 0xd2fee62e, 0x00ffd124, 
    0x45f74faa, 0xd546913a, 0x4b31e80b, 0xdb9d4c5e, 0x5be19e39, 0x6e892f5a, 0x82675b34, 0x3b902318, 0xcfc925ef, 0x62a53f40, 0x6c6d42d1, 0x1dd5da34, 
    0xd43c9e44, 0x684b0819, 0xe4468f11, 0x6eaef5e3, 0x9db9e579, 0xc8cb99e7, 0xb9c72ce7, 0xc9a1e8a8, 0xdee4c6bd, 0xb45975e6, 0xabc23ffe, 0xe34f9b55, 
    0xc4d62bfc, 0x9b0900ff, 0x144bf8d4, 0x07794551, 0xaca53531, 0x6d29b7f7, 0x7fe59b6c, 0x3c03b9ba, 0x3fada967, 0xdf7544f8, 0x00fff1f9, 0x34fea9c8, 
    0x00ff0adf, 0xfe67cbc8, 0x26e87ff3, 0x0dcb9bbb, 0x1b5e6d4e, 0xc9437d8b, 0xe2bb40b6, 0xe7c1eee4, 0xd28a9c8e, 0xd65c4d31, 0x3c575210, 0xeac6f2da, 
    0xea266fc2, 0x9cf18916, 0xba827a37, 0xafbc1edf, 0xdda66c73, 0x01c338d2, 0x9c1cc629, 0xb962f871, 0x3b2b992a, 0x63672511, 0x3a6bb43a, 0x19b76bb4, 
    0x95667ef6, 0x5114057a, 0x9526065c, 0x5db5879f, 0x6d6ed942, 0x9bc87c6d, 0x46316f38, 0x9ad07170, 0xf586cf75, 0x695a488b, 0x8c2298ac, 0x81adac92, 
    0x7f5d13f8, 0x6f96a386, 0x49702c07, 0x40e24ae5, 0xb8d3fd8f, 0x48cbd5e0, 0x493b1d75, 0xb652ed9b, 0x55c938a3, 0xe7744c5f, 0xaca06cad, 0x8fa6a96e, 
    0x9495a230, 0x416015ab, 0xd41e0407, 0x5e818995, 0x00fff8d3, 0xed55b10a, 0xabf08f3f, 0xfe89d115, 0xc4a72e2b, 0xaca53515, 0x6d29b7f7, 0x7fe59b6c, 
    0x3c03b9ba, 0x6ba8a967, 0xf2bfc25f, 0x00ffd932, 0x09fadfbc, 0x25bb12ac, 0xff0ebb2b, 0x5d478400, 0x1f9f00ff, 0xe39f8afc, 0x3796b759, 0x37791356, 
    0x8c4fb450, 0xa4d7bbe1, 0x736a58de, 0x5bdcf06a, 0xb24d1eea, 0x2717df05, 0x743c0f76, 0xe33957e4, 0x6dee95d7, 0x47badb94, 0x38256018, 0x3f8e93c3, 
    0x8292560c, 0xa6b2e64a, 0x51c9b992, 0x88911545, 0x00144551, 0x879f9556, 0xd9425db5, 0x7c6d6d6e, 0x6f389bc8, 0x71704631, 0xafcd9ad0, 0x72d4f043, 
    0x8ee5e0cd, 0x5ca93c09, 0x00ff1148, 0x1a1c77ba, 0xb8ec46b8, 0x9cd94945, 0xf586cf85, 0x695a488b, 0x8c2298ac, 0x81adac92, 0x755913f8, 0xea90f6e9, 
    0x3793763a, 0x466da5da, 0xbeaa9271, 0x5acfe998, 0xac5216f3, 0x1c048155, 0x38517b10, 0x8d8a73d8, 0x2a8aa284, 0xb3eacc0c, 0x857ffc69, 0x9f36ab56, 
    0xaf57f8c7, 0x3a13fe89, 0x8a257c6a, 0x83bca228, 0xc4f0d598, 0xad881789, 0x5054471e, 0x806396cd, 0x4e5d533e, 0xea58a4b3, 0x1d5e92fa, 0x20362866, 
    0x9e54aa5d, 0x77073909, 0x812b7d1d, 0xca4aada2, 0x5676c6c5, 0x633cec68, 0x654fd9ab, 0xc2b48515, 0xce0c5777, 0x636020a7, 0xf471adaf, 0xee4e4a51, 
    0x9db994e2, 0xacd1eacc, 0xdcaed1ea, 0x9af9d967, 0x5114e855, 0x9d187045, 0xb3d680e6, 0x52ec3ff8, 0x4a0bc55d, 0xc32c21b2, 0xc679922b, 0x0d264d6a, 
    0x4f30c337, 0x4ec2ba24, 0x10940f1c, 0x098a1e07, 0x8afebce6, 0xd5c89fd3, 0x2cb4b554, 0xe6addc5f, 0xab7271a1, 0x989165b5, 0x574d400f, 0xc8cc8aa2, 
    0x7ffc69af, 0xf6aa5885, 0x55f8c79f, 0xffc4e88a, 0x53971500, 0xf0d50ae2, 0x881789c4, 0x54471ead, 0x6396cd50, 0x59533e80, 0xd9095654, 0xeeec94dc, 
    0x91ceda77, 0x49eaab63, 0xa0987578, 0xa97681d8, 0xe4247852, 0xf475dc1d, 0xd5313eaa, 0x8ab2a7ec, 0x3b61dac2, 0x536786ab, 0xd7313090, 0x2afab8d6, 
    0x758bfcdc, 0xc22d7b2f, 0xccac288a, 0xa0288ac2, 0x01cdbb02, 0x7ff067ad, 0x8abba4d8, 0x42649516, 0x24578659, 0x07d78cf3, 0x62655445, 0x7a562ea3, 
    0x9b069306, 0x922798e1, 0x0e27615d, 0x0308ca07, 0xf304458f, 0xcafd3d5c, 0x17176ade, 0x5956bb2a, 0x04f48019, 0x72287ad5, 0x2be5b0ba, 0x511405ab, 
    0xd9ff4152, 
};
};
} // namespace BluePrint
