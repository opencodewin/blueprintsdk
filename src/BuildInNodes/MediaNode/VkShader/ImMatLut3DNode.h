#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Lut3D.h"
#define USE_BOOKMARK
#include <ImGuiFileDialog.h>

namespace BluePrint
{
struct Lut3DNode final : Node
{
    BP_NODE_WITH_NAME(Lut3DNode, "Lut 3D", "CodeWin", VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Filter#Video#Color")
    Lut3DNode(BP* blueprint): Node(blueprint) { m_Name = "Lut 3D"; }
    ~Lut3DNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device || m_setting_changed)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                if (m_lut_mode != NO_DEFAULT)
                    m_filter = new ImGui::LUT3D_vulkan(m_lut_mode, m_interpolation_mode, gpu);
                else if (!m_path.empty())
                    m_filter = new ImGui::LUT3D_vulkan(m_path, m_interpolation_mode, gpu);
                m_setting_changed = false;
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            bool is_hdr_pq = (m_lut_mode == SDR709_HDRPQ) || (m_lut_mode == HDRHLG_HDRPQ);
            bool is_hdr_hlg =(m_lut_mode == SDR709_HDRHLG) || (m_lut_mode == HDRPQ_HDRHLG);
            bool is_sdr_709 =(m_lut_mode == HDRHLG_SDR709) || (m_lut_mode == HDRPQ_SDR709);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            im_RGB.color_space = (is_hdr_pq || is_hdr_hlg) ? IM_CS_BT2020 : 
                                is_sdr_709 ? IM_CS_BT709 : IM_CS_SRGB; // 601?
            if (is_hdr_pq) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_PQ;
            if (is_hdr_hlg) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_HLG;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        int lut_mode = m_lut_mode;
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::RadioButton("SDR->HLG",  (int *)&lut_mode, SDR709_HDRHLG); ImGui::SameLine();
        ImGui::RadioButton("SDR->PQ",   (int *)&lut_mode, SDR709_HDRPQ); ImGui::SameLine();
        ImGui::RadioButton("HLG->SDR",  (int *)&lut_mode, HDRHLG_SDR709); ImGui::SameLine();
        ImGui::RadioButton("PQ->SDR",   (int *)&lut_mode, HDRPQ_SDR709); ImGui::SameLine();
        ImGui::RadioButton("HLG->PQ",   (int *)&lut_mode, HDRHLG_HDRPQ); ImGui::SameLine();
        ImGui::RadioButton("PQ->HLG",   (int *)&lut_mode, HDRPQ_HDRHLG); ImGui::SameLine();
        ImGui::RadioButton("File",      (int *)&lut_mode, NO_DEFAULT);
        ImGui::Separator();
        ImGui::RadioButton("Nearest",       (int *)&m_interpolation_mode, IM_INTERPOLATE_NEAREST); ImGui::SameLine();
        ImGui::RadioButton("Trilinear",     (int *)&m_interpolation_mode, IM_INTERPOLATE_TRILINEAR); ImGui::SameLine();
        ImGui::RadioButton("Teteahedral",   (int *)&m_interpolation_mode, IM_INTERPOLATE_TETRAHEDRAL);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
        ImGui::Separator();
        if (m_lut_mode != lut_mode)
        {
            m_lut_mode = lut_mode;
            m_setting_changed = true;
        }
        // open from file
        ImVec2 minSize = ImVec2(400, 300);
		ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        if (m_lut_mode == NO_DEFAULT) ImGui::BeginDisabled(false);  else ImGui::BeginDisabled(true);
        static string filters = ".cube";
        ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_Modal;
        vflags |= ImGuiFileDialogFlags_DontShowHiddenFiles;
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose File ## "))
            ImGuiFileDialog::Instance()->OpenDialog("##NodeChooseLutFileDlgKey", "Choose File", 
                                                    filters.c_str(), 
                                                    m_path.empty() ? "." : m_path,
                                                    1, this, vflags);
        if (ImGuiFileDialog::Instance()->Display("##NodeChooseLutFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_path = ImGuiFileDialog::Instance()->GetFilePathName();
                m_file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                m_setting_changed = true;
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        if (m_lut_mode != NO_DEFAULT)
        {
            m_path.clear();
            m_file_name.clear();
        }
        ImGui::SameLine(0);
        ImGui::TextUnformatted(m_file_name.c_str());
        ImGui::EndDisabled();
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        // Draw custom layout
        bool changed = false;
        int lut_mode = m_lut_mode;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::TextUnformatted("Please using Node Setting to change mode");
        ImGui::TextUnformatted("Current mode:");
        if (!m_file_name.empty())
            ImGui::Text("Lut from file: %s", m_file_name.c_str());
        else
        {
            switch (m_lut_mode)
            {
                case SDR709_HDRHLG: ImGui::TextUnformatted("SDR to HLG"); break;
                case SDR709_HDRPQ: ImGui::TextUnformatted("SDR to PQ"); break;
                case HDRHLG_SDR709: ImGui::TextUnformatted("HLG to SDR"); break;
                case HDRPQ_SDR709: ImGui::TextUnformatted("PQ to SDR"); break;
                case HDRHLG_HDRPQ: ImGui::TextUnformatted("HLG to PQ"); break;
                case HDRPQ_HDRHLG: ImGui::TextUnformatted("PQ to HDR"); break;
                default: ImGui::TextUnformatted("Unknown Lut"); break;
            }
        }

        switch (m_interpolation_mode)
        {
            case IM_INTERPOLATE_NEAREST: ImGui::TextUnformatted("Interpolate Nearest"); break;
            case IM_INTERPOLATE_TRILINEAR: ImGui::TextUnformatted("Interpolate Trilinear"); break;
            case IM_INTERPOLATE_TETRAHEDRAL: ImGui::TextUnformatted("Interpolate Tetrahedral"); break;
            default: ImGui::TextUnformatted("Unknown Interpolate"); break;
        }   

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
        if (value.contains("lut_mode"))
        {
            auto& val = value["lut_mode"];
            if (val.is_number()) 
                m_lut_mode = val.get<imgui_json::number>();
        }
        if (value.contains("interpolation"))
        {
            auto& val = value["interpolation"];
            if (val.is_number()) 
                m_interpolation_mode = val.get<imgui_json::number>();
        }
        if (value.contains("lut_file_path"))
        {
            auto& val = value["lut_file_path"];
            if (val.is_string())
            {
                m_path = val.get<imgui_json::string>();
            }
        }
        if (value.contains("lut_file_name"))
        {
            auto& val = value["lut_file_name"];
            if (val.is_string())
            {
                m_file_name = val.get<imgui_json::string>();
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["lut_mode"] = imgui_json::number(m_lut_mode);
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
        value["lut_file_path"] = m_path;
        value["lut_file_name"] = m_file_name;
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        float font_size = ImGui::GetFontSize();
        float size_min = size.x > size.y ? size.y : size.x;
        ImGui::SetWindowFontScale((size_min - 16) / font_size);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
#if IMGUI_ICONS
        ImGui::Button((std::string(u8"\ue3b7") + "##" + std::to_string(m_ID)).c_str(), size);
#else
        ImGui::Button((std::string("F") + "##" + std::to_string(m_ID)).c_str(), size);
#endif
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_lut_mode {SDR709_HDRHLG};
    int m_interpolation_mode {IM_INTERPOLATE_TRILINEAR};
    string  m_path;
    string m_file_name;
    ImGui::LUT3D_vulkan * m_filter {nullptr};
    bool  m_setting_changed {false};
};
} // namespace BluePrint
