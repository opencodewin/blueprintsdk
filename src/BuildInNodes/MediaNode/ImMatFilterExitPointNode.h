#pragma once
#include <imgui.h>
namespace BluePrint
{
struct FilterExitPointNode final : Node
{
    BP_NODE(FilterExitPointNode, VERSION_BLUEPRINT, NodeType::ExitPoint, NodeStyle::Simple, "System")

    FilterExitPointNode(BP& blueprint): Node(blueprint) { m_Name = "End"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat = context.GetPinValue(m_MatIn);
        m_MatIn.SetValue(mat);
        context.m_Callstack.clear();
        return {};
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkInputDataPin() override { return &m_MatIn; }

    FlowPin m_Enter = { this, "End" };
    MatPin  m_MatIn = { this, "In" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
};
} // namespace BluePrint