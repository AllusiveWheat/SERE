#pragma once
#include "TransformNodes.h"

// Membership is graph identity, never evaluation order or canvas position.
struct TransformGroupMember {
    ImFlow::NodeUID node;
    std::string pin;
};

class TransformGroupNode : public RuiBaseNode {
public:
    static inline std::string name = "Transform Group";
    static inline std::string category = "Transform";
    std::string label = "Transform Group";
    std::vector<TransformGroupMember> members;
    std::string error;
    uint64_t frameHash = randomInt64();
    uint64_t rootHash = randomInt64();
    std::string sizeName = Variable::UniqueName();
    bool measuring = false;
    std::string memberSearch;

    TransformGroupNode(const std::shared_ptr<RenderInstance>&, ImFlow::StyleManager&);
    TransformGroupNode(const std::shared_ptr<RenderInstance>&, ImFlow::StyleManager&,
        rapidjson::GenericObject<false, rapidjson::Value>);
    static std::vector<std::shared_ptr<ImFlow::PinProto>> GetPinInfo();
    void draw() override;
    void Serialize(rapidjson::Value&, rapidjson::Document::AllocatorType&) override;
    void Export(RuiExportPrototype&) override;
    void AddSelected();
    bool CanAddNode(ImFlow::BaseNode& node);
    void AddNode(ImFlow::BaseNode& node);
    void RemapMembers(const std::map<ImFlow::NodeUID, ImFlow::NodeUID>&);
    std::vector<TransformResult> ReadMembers();
    TransformSize Measure();
    TransformResult Frame();
    void ApplyPreview();
};

// Validate before requesting pin values, so feedback is diagnosed before lazy
// evaluation can return an old cached value from a recursive graph.
void ValidateTransformGroups(ImFlow::ImNodeFlow& graph);
void PrepareTransformGroupExport(RuiExportPrototype& proto);
