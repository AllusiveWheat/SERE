#include "TransformGroupNode.h"
#include "RuiRendering/TransformGroupMath.h"
#include "Imgui/imgui_stdlib.h"
#include <cmath>
#include <iterator>
#include <stdexcept>

// Group callbacks own contiguous measurement members and placement records.
// Type-12 commands are deferred until all ordinary transforms are emitted.
void PrepareTransformGroupExport(RuiExportPrototype& proto) {
    if (proto.transformGroups.empty()) return;
    std::vector<uint64_t> originalOrder;
    originalOrder.reserve(proto.transformCallbacks.size());
    for (const auto& callback : proto.transformCallbacks) originalOrder.push_back(callback.identifier);
    std::vector<ExportElement<uint64_t>> operations;
    std::set<uint64_t> schedulingDependencies;
    std::map<uint64_t, ExportElement<uint64_t>> members;
    std::vector<uint64_t> memberOrder;
    for (const auto& group : proto.transformGroups) {
        proto.transformProducers.emplace(group.frameHash, group.frameHash);
        for (auto hash : group.members)
            if (!proto.transformProducers.emplace(hash, group.frameHash).second)
                throw std::runtime_error(group.label + ": overlapping group membership.");
    }
    // A transform using Content Size must wait for the group that computes it.
    std::function<void(const std::string&, std::set<uint64_t>&, std::set<std::string>&)> addSizeDependency;
    addSizeDependency = [&](const std::string& name, auto& dependencies, auto& visited) {
        if (name.empty() || !visited.insert(name).second) return;
        for (const auto& group : proto.transformGroups)
            if (group.sizeName == name) dependencies.insert(group.frameHash);
        for (const auto& code : proto.codeElements) if (code.identifier == name)
            for (const auto& dependency : code.dependencys) addSizeDependency(dependency, dependencies, visited);
    };
    for (auto& callback : proto.transformCallbacks) {
        std::set<std::string> visited;
        for (const auto& name : callback.codeDependencies) addSizeDependency(name, callback.dependencys, visited);
        if (proto.transformProducers.contains(callback.identifier)) {
            members.emplace(callback.identifier, callback);
            memberOrder.push_back(callback.identifier);
        }
    }
    std::erase_if(proto.transformCallbacks, [&](const auto& callback) { return members.contains(callback.identifier); });
    for (auto group : proto.transformGroups) {
        const uint16_t frameIndex = static_cast<uint16_t>(proto.transformIndices.size());
        proto.transformIndices.emplace(group.rootHash, frameIndex);
        uint8_t type = 0;
        proto.AddTransformData(&type, 1);
        ExportElement<std::string> init;
        init.identifier = Variable::UniqueName();
        const auto rootSize = group.rootSize;
        init.dependencys = {rootSize.name};
        init.callback = [frameIndex, rootSize](RuiExportPrototype& p) {
            p.codeLines.push_back(std::format("transformSize[{}] = {};", frameIndex, rootSize.GetFormattedName(p)));
        };
        proto.transformCodeDependencies.insert(init.identifier);
        proto.codeElements.push_back(init);

        ExportElement<uint64_t> operation;
        operation.identifier = group.frameHash;
        operation.dependencys = {group.parentHash};
        schedulingDependencies.insert(group.members.begin(), group.members.end());
        std::vector<ExportElement<uint64_t>> block;
        std::set<uint64_t> ordered;
        while (block.size() < group.members.size()) {
            const auto previousSize = block.size();
            for (auto hash : memberOrder) {
                if (!group.members.contains(hash)) continue;
                if (ordered.contains(hash)) continue;
                if (!members.contains(hash)) throw std::runtime_error(group.label + ": member cannot be exported.");
                const auto& member = members.at(hash);
                bool ready = true;
                for (auto dependency : member.dependencys)
                    if (group.members.contains(dependency) && !ordered.contains(dependency)) ready = false;
                if (!ready) continue;
                for (auto dependency : member.dependencys)
                    if (!group.members.contains(dependency)) operation.dependencys.insert(dependency);
                block.push_back(member); ordered.insert(hash);
            }
            if (block.size() == previousSize) throw std::runtime_error(group.label + ": cyclic member dependencies.");
        }
        operation.callback = [group, block](RuiExportPrototype& proto) mutable {
            group.begin = static_cast<uint16_t>(proto.transformIndices.size());
            for (const auto& member : block) {
                const auto firstCode = proto.codeElements.size();
                member.callback(proto);
                proto.transformCodeDependencies.insert(member.codeDependencies.begin(), member.codeDependencies.end());
                for (size_t i = firstCode; i < proto.codeElements.size(); ++i)
                    proto.transformCodeDependencies.insert(proto.codeElements[i].identifier);
            }
            group.end = static_cast<uint16_t>(proto.transformIndices.size());
            if (block.empty()) {
                group.begin = proto.transformIndices.at(group.rootHash);
                group.end = static_cast<uint16_t>(group.begin + 1);
            }
            auto barrier = [&](ExportElement<std::string> code) {
                code.dependencys.insert(proto.transformCodeDependencies.begin(), proto.transformCodeDependencies.end());
                proto.transformCodeDependencies.insert(code.identifier);
                proto.codeElements.push_back(std::move(code));
            };
            ExportElement<std::string> bounds;
            bounds.identifier = group.sizeName;
            const auto memberEnd = proto.transformData.size();
            bounds.callback = [group, memberEnd](RuiExportPrototype& p) {
                if (memberEnd > p.lastExecutedTransformOffset) p.EmitTransformExecution(memberEnd);
                p.codeLines.push_back(std::format("__m128 {} = funcs->rebaseRuiTransformBounds(inst,{},{});", group.sizeName, group.begin, group.end));
            };
            barrier(bounds);
            // Retail consumes the measured size in a later type-4 placement,
            // then uses that record directly as the type-12 reference.
            const uint16_t frameIndex = static_cast<uint16_t>(proto.transformIndices.size());
            proto.transformIndices.emplace(group.frameHash, frameIndex);
            ExportElement<std::string> sizeCode;
            sizeCode.identifier = Variable::UniqueName();
            sizeCode.dependencys = {group.sizeName};
            sizeCode.callback = [group, frameIndex](RuiExportPrototype& p) {
                p.codeLines.push_back(std::format("transformSize[{}] = {};", frameIndex, group.sizeName));
            };
            proto.transformCodeDependencies.insert(sizeCode.identifier);
            proto.codeElements.push_back(sizeCode);
            struct Placement { uint8_t type = 4, count = 1; uint16_t parent; Float2Offsets position, pivot; } placement;
            static_assert(sizeof(Placement) == 12);
            placement.parent = proto.transformIndices.at(group.parentHash);
            placement.position = proto.GetFloat2DataVariableOffset(group.position);
            placement.pivot = proto.GetFloat2DataVariableOffset(group.pivot);
            proto.AddTransformData(reinterpret_cast<uint8_t*>(&placement), sizeof(placement));
            const auto rootIndex = proto.transformIndices.at(group.rootHash);
            proto.pendingTransform12.push_back({frameIndex, rootIndex, static_cast<uint16_t>(rootIndex + 1)});
        };
        schedulingDependencies.insert(operation.dependencys.begin(), operation.dependencys.end());
        operations.push_back(std::move(operation));
    }
    // Preserve ordinary command order through the final group's measurement
    // source or placement parent. Later independent transforms follow the frames.
    size_t anchor = 0;
    for (size_t i = 0; i < originalOrder.size(); ++i)
        if (schedulingDependencies.contains(originalOrder[i])) anchor = i + 1;
    size_t insertion = 0;
    for (size_t i = 0; i < anchor; ++i)
        if (!members.contains(originalOrder[i])) ++insertion;
    proto.transformCallbacks.insert(proto.transformCallbacks.begin() + insertion,
        std::make_move_iterator(operations.begin()), std::make_move_iterator(operations.end()));
}

namespace {
ImFlow::Pin* MemberPin(ImFlow::ImNodeFlow& graph, const TransformGroupMember& member) {
    auto node = graph.getNodes().find(member.node);
    if (node == graph.getNodes().end()) return nullptr;
    for (const auto& pin : node->second->getOuts())
        if (pin->getName() == member.pin && pin->getDataType() == typeid(TransformResult)) return pin.get();
    return nullptr;
}
}

TransformGroupNode::TransformGroupNode(const std::shared_ptr<RenderInstance>& render, ImFlow::StyleManager& style)
    : RuiBaseNode(name, category, GetPinInfo(), render, style) {
    getIn<TransformResult>("Parent")->setEmptyVal(render->transformResults[2]);
    getOut<TransformSize>("Content Size")->behaviour([this] { return Measure(); });
    getOut<TransformResult>("Frame")->behaviour([this] { return Frame(); });
    getOut<TransformResult>("Content Root")->behaviour([this] {
        const auto size = getInVal<TransformSize>("Root Size").size;
        return TransformResult(_mm_and_ps(size, _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, -1))),
            _mm_setzero_ps(), size, rootHash);
    });
}

TransformGroupNode::TransformGroupNode(const std::shared_ptr<RenderInstance>& render, ImFlow::StyleManager& style,
    rapidjson::GenericObject<false, rapidjson::Value> obj) : TransformGroupNode(render, style) {
    if (obj.HasMember("Label") && obj["Label"].IsString()) label = obj["Label"].GetString();
    if (obj.HasMember("Members") && obj["Members"].IsArray())
        for (auto& member : obj["Members"].GetArray())
            if (member.IsObject() && member.HasMember("Node") && member["Node"].IsUint64()
                && member.HasMember("Pin") && member["Pin"].IsString())
                members.push_back({member["Node"].GetUint64(), member["Pin"].GetString()});
}

std::vector<std::shared_ptr<ImFlow::PinProto>> TransformGroupNode::GetPinInfo() {
    return {
        std::make_shared<ImFlow::InPinProto<TransformResult>>("Parent", ImFlow::ConnectionFilter::SameType(), TransformResult()),
        std::make_shared<ImFlow::InPinProto<TransformSize>>("Root Size", ImFlow::ConnectionFilter::SameType(), TransformSize(_mm_set1_ps(1.f))),
        std::make_shared<ImFlow::InPinProto<Float2Variable>>("Position", ImFlow::ConnectionFilter::SameType(), Float2Variable(0, 0)),
        std::make_shared<ImFlow::InPinProto<Float2Variable>>("Pivot", ImFlow::ConnectionFilter::SameType(), Float2Variable(0, 0)),
        std::make_shared<ImFlow::OutPinProto<TransformSize>>("Content Size"),
        std::make_shared<ImFlow::OutPinProto<TransformResult>>("Frame"),
        std::make_shared<ImFlow::OutPinProto<TransformResult>>("Content Root")
    };
}
std::vector<TransformResult> TransformGroupNode::ReadMembers() {
    std::vector<TransformResult> values;
    for (const auto& member : members) {
        auto pin = MemberPin(*getHandler(), member);
        if (!pin) throw std::runtime_error(label + ": missing member transform");
        values.push_back(static_cast<ImFlow::OutPin<TransformResult>*>(pin)->val());
    }
    return values;
}

TransformSize TransformGroupNode::Measure() {
    if (!error.empty() || measuring) return TransformSize(_mm_setzero_ps(), sizeName);
    measuring = true;
    __m128 size = _mm_setzero_ps();
    try {
        auto values = members.empty()
            ? std::vector<TransformResult>{getOut<TransformResult>("Content Root")->val()}
            : ReadMembers();
        size = RebaseTransformGroup(values);
    }
    catch (const std::exception& ex) { error = ex.what(); }
    measuring = false;
    return TransformSize(size, sizeName);
}

TransformResult TransformGroupNode::Frame() {
    TransformResult result;
    result.hash = frameHash;
    result.inputSize = Measure().size;
    if (!error.empty()) return result;
    const auto parent = getInVal<TransformResult>("Parent");
    result.directionVector = _mm_and_ps(_mm_mul_ps(_mm_div_ps(result.inputSize, parent.inputSize), parent.directionVector),
        _mm_cmpneq_ps(_mm_setzero_ps(), result.inputSize));
    const auto position = getInVal<Float2Variable>("Position").value;
    const auto pivot = getInVal<Float2Variable>("Pivot").value;
    const __m128 offset = _mm_mul_ps(_mm_set_ps(position.y, position.y, position.x, position.x), parent.directionVector);
    const __m128 origin = _mm_mul_ps(_mm_set_ps(pivot.y, pivot.y, pivot.x, pivot.x), result.directionVector);
    result.position = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_shuffle_ps(offset, offset, 78), offset), parent.position),
        _mm_add_ps(_mm_shuffle_ps(origin, origin, 78), origin));
    return result;
}

void TransformGroupNode::ApplyPreview() {
    if (!error.empty()) return;
    auto values = ReadMembers();
    RebaseTransformGroup(values);
    const auto frame = Frame();
    std::vector<TransformResult> root{getOut<TransformResult>("Content Root")->val()};
    ApplyTransform12(root, frame);
    for (const auto& value : values) render->finalizedTransforms.insert_or_assign(value.hash, value);
    render->finalizedTransforms.insert_or_assign(root.front().hash, root.front());
    render->finalizedTransforms.insert_or_assign(frame.hash, frame);
}

bool TransformGroupNode::CanAddNode(ImFlow::BaseNode& node) {
    if (node.getUID() == getUID() || dynamic_cast<TransformGroupNode*>(&node)) return false;
    if (std::none_of(node.getOuts().begin(), node.getOuts().end(),
        [](const auto& pin) { return pin->getDataType() == typeid(TransformResult); })) return false;
    for (const auto& [id, candidate] : getHandler()->getNodes()) {
        if (auto group = dynamic_cast<TransformGroupNode*>(candidate.get()))
            if (std::any_of(group->members.begin(), group->members.end(),
                [&](const auto& member) { return member.node == node.getUID(); })) return false;
    }
    return true;
}

void TransformGroupNode::AddNode(ImFlow::BaseNode& node) {
    if (!CanAddNode(node)) return;
    for (const auto& pin : node.getOuts())
        if (pin->getDataType() == typeid(TransformResult)) members.push_back({node.getUID(), pin->getName()});
    std::sort(members.begin(), members.end(), [](const auto& a, const auto& b) { return std::tie(a.node, a.pin) < std::tie(b.node, b.pin); });
}

void TransformGroupNode::AddSelected() {
    for (const auto& [id, node] : getHandler()->getNodes())
        if (node->isSelected()) AddNode(*node);
}

void TransformGroupNode::RemapMembers(const std::map<ImFlow::NodeUID, ImFlow::NodeUID>& remap) {
    std::erase_if(members, [&](const auto& m) { return !remap.contains(m.node); });
    for (auto& member : members) member.node = remap.at(member.node);
}

void TransformGroupNode::draw() {
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Name", &label);
    if (ImGui::Button("Add transforms...")) {
        memberSearch.clear();
        ImGui::OpenPopup("Add group transforms");
    }
    if (ImGui::BeginPopup("Add group transforms")) {
        ImGui::SetNextItemWidth(280);
        ImGui::InputTextWithHint("##search", "Search transforms by name or ID", &memberSearch);
        ImGui::TextUnformatted("Choose transforms to add. Click outside when done.");
        std::map<ImFlow::NodeUID, std::shared_ptr<ImFlow::BaseNode>> candidates(
            getHandler()->getNodes().begin(), getHandler()->getNodes().end());
        bool found = false;
        ImGui::BeginChild("Available transforms", ImVec2(350, 240));
        for (const auto& [id, node] : candidates) {
            if (!CanAddNode(*node)) continue;
            const auto text = std::format("{} ({})", node->getTitle(), id);
            if (!memberSearch.empty() && !caseInsensitiveSearch(text, memberSearch)) continue;
            found = true;
            if (ImGui::Selectable(text.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) AddNode(*node);
            if (ImGui::IsItemHovered()) {
                const auto pos = node->getPos();
                ImGui::SetTooltip("Canvas position: %.0f, %.0f", pos.x, pos.y);
            }
        }
        if (!found) ImGui::TextUnformatted("No matching ungrouped transforms.");
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    if (ImGui::Button("Add selected transforms")) AddSelected();
    ImGui::TextUnformatted("Type 12 moves Content Root only; members provide bounds.");
    if (ImGui::TreeNode("Members", "Members (%zu)", members.size())) {
        for (size_t i = 0; i < members.size();) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("Remove")) { members.erase(members.begin() + i); ImGui::PopID(); continue; }
            ImGui::SameLine();
            auto node = getHandler()->getNodes().find(members[i].node);
            const auto title = node == getHandler()->getNodes().end() ? "Missing transform" : node->second->getTitle();
            if (ImGui::Selectable(std::format("{} / {}", title, members[i].pin).c_str())) {
                if (node != getHandler()->getNodes().end()) node->second->selected(true);
            }
            ImGui::PopID();
            ++i;
        }
        ImGui::TreePop();
    }
    if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
    else { float size[4]; _mm_storeu_ps(size, Measure().size); ImGui::Text("Bounds: %.4g x %.4g", size[0], size[2]); }
}

void TransformGroupNode::Serialize(rapidjson::Value& obj, rapidjson::Document::AllocatorType& allocator) {
    obj.AddMember("Name", name, allocator);
    obj.AddMember("Category", category, allocator);
    obj.AddMember("Label", label, allocator);
    rapidjson::Value entries(rapidjson::kArrayType);
    for (const auto& member : members) {
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember("Node", static_cast<uint64_t>(member.node), allocator);
        entry.AddMember("Pin", member.pin, allocator);
        entries.PushBack(entry, allocator);
    }
    obj.AddMember("Members", entries, allocator);
    RuiBaseNode::Serialize(obj, allocator);
}

void TransformGroupNode::Export(RuiExportPrototype& proto) {
    if (!error.empty()) throw std::runtime_error(label + ": " + error);
    RuiExportPrototype::TransformGroupExport group;
    group.label = label; group.sizeName = sizeName; group.frameHash = frameHash; group.rootHash = rootHash;
    group.parentHash = getInVal<TransformResult>("Parent").hash;
    group.rootSize = getInVal<TransformSize>("Root Size");
    for (const auto& value : ReadMembers()) group.members.insert(value.hash);
    group.position = getInVal<Float2Variable>("Position");
    group.pivot = getInVal<Float2Variable>("Pivot");
    proto.AddDataVariable(group.position);
    proto.AddDataVariable(group.pivot);
    proto.transformGroups.push_back(group);
}

void ValidateTransformGroups(ImFlow::ImNodeFlow& graph) {
    std::map<ImFlow::NodeUID, TransformGroupNode*> owners;
    std::vector<TransformGroupNode*> groups;
    for (auto& [id, node] : graph.getNodes()) if (auto group = dynamic_cast<TransformGroupNode*>(node.get())) {
        group->error.clear(); groups.push_back(group);
    }
    for (auto group : groups) {
        for (const auto& member : group->members) {
            if (!MemberPin(graph, member)) { group->error = "A member was deleted. Remove its entry or restore the node."; continue; }
            if (dynamic_cast<TransformGroupNode*>(graph.getNodes().at(member.node).get())) { group->error = "Nested groups are not supported yet."; continue; }
            if (auto existing = owners.find(member.node); existing != owners.end()) {
                existing->second->error = group->error = "A transform belongs to more than one group.";
            } else owners.emplace(member.node, group);
        }
    }
    // Follow all input edges, not just transform edges: bounds can feed math
    // which feeds a member's size and creates the same feedback cycle.
    for (auto group : groups) {
        std::set<ImFlow::NodeUID> visited, active;
        std::function<void(ImFlow::BaseNode*)> visit = [&](ImFlow::BaseNode* node) {
            if (active.contains(node->getUID())) { group->error = "Cyclic input dependency in group contents."; return; }
            if (!visited.insert(node->getUID()).second) return;
            if (dynamic_cast<TransformGroupNode*>(node)) { group->error = "Group contents cannot depend on group outputs."; return; }
            if (auto owner = owners.find(node->getUID()); owner != owners.end() && owner->second != group) {
                group->error = "Cross-group transform dependencies are not supported yet."; return;
            }
            active.insert(node->getUID());
            for (const auto& pin : node->getIns()) if (pin->isConnected()) {
                auto source = pin->getLink().lock()->left();
                if (source->getParent() == group && source->getName() == "Content Root") continue;
                visit(source->getParent());
            }
            active.erase(node->getUID());
        };
        for (const auto& member : group->members) if (graph.getNodes().contains(member.node)) visit(graph.getNodes().at(member.node).get());
    }
    for (const auto& [id, node] : graph.getNodes()) {
        if (dynamic_cast<TransformGroupNode*>(node.get())) continue;
        const bool producesTransform = std::any_of(node->getOuts().begin(), node->getOuts().end(),
            [](const auto& pin) { return pin->getDataType() == typeid(TransformResult); });
        if (!producesTransform) continue;
        for (const auto& pin : node->getIns()) if (pin->isConnected()) {
            auto source = pin->getLink().lock()->left();
            if (source->getDataType() != typeid(TransformResult)) continue;
            auto owner = owners.find(source->getParent()->getUID());
            if (owner != owners.end() && (!owners.contains(id) || owners.at(id) != owner->second))
                owner->second->error = "Add dependent transforms to this group; member outputs may connect directly to render nodes.";
        }
    }
    for (auto group : groups) {
        std::set<ImFlow::NodeUID> visited;
        std::function<void(ImFlow::Pin*)> visit = [&](ImFlow::Pin* source) {
            auto node = source->getParent();
            if (auto other = dynamic_cast<TransformGroupNode*>(node)) {
                if (other == group && source->getName() != "Content Size")
                    group->error = "Placement cannot depend on its own Frame output.";
                return;
            }
            if (!visited.insert(node->getUID()).second) return;
            if (owners.contains(node->getUID())) { group->error = "Placement cannot depend on member transforms."; return; }
            for (const auto& pin : node->getIns()) if (pin->isConnected()) visit(pin->getLink().lock()->left());
        };
        for (const auto& pin : group->getIns()) if (pin->isConnected()) visit(pin->getLink().lock()->left());
    }
}
