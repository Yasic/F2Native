#include "graphics/geom/Interval.h"
#include "graphics/XChart.h"
#include "graphics/scale/Scale.h"
#include "graphics/scale/continuous/Linear.h"

using namespace xg;

float geom::Interval::GetDefaultWidthRatio(XChart &chart) {
    if(chart.coord_->GetType() == coord::CoordType::Polar) {
        const std::string &xField = GetXScaleField();
        scale::AbstractScale &xScale = chart.GetScale(xField);
        size_t count = xScale.values.size();
        return (chart.coord_->IsTransposed() && count > 1) ? 0.75f : 1.0f;
    }
    return 0.5f;
}

nlohmann::json geom::Interval::CreateShapePointsCfg(XChart &chart, nlohmann::json &item, size_t index) {

    nlohmann::json rst;
    const std::string &xField = GetXScaleField();
    const std::string &yField = GetYScaleField();

    scale::AbstractScale &xScale = chart.GetScale(xField);
    scale::AbstractScale &yScale = chart.GetScale(yField);

    double x = xScale.Scale(item[xField]);
    if(item[yField].is_array()) {
        vector<double> y;
        vector<double> stack_item = item[yField];
        for(size_t i = 0; i < stack_item.size(); i++) {
            double y_d = stack_item[i];
            double y_s = yScale.Scale(y_d);
            y.push_back(y_s);
        }
        rst["y"] = y;
    } else {
        double y = yScale.Scale(item[yField]);
        rst["y"] = y;
    }
    double y0 = 0;
    if(yScale.GetType() == scale::ScaleType::Linear) {
        y0 = yScale.rangeMin;
    }
    std::size_t count = fmax(xScale.GetValuesSize(), 1);
    double normalizeSize = 1.0f;
    //默认系数
    float widthRatio = GetDefaultWidthRatio(chart);
    if(attrs_.find(AttrType::Size) != attrs_.end()) {
        AttrBase &attr = *attrs_[AttrType::Size].get();
        const attr::Size &size = static_cast<const attr::Size &>(attr);
        widthRatio = size.GetSize(0) * chart.GetCanvasContext().GetDevicePixelRatio() / chart.GetCoord().GetWidth();
    } else {
        normalizeSize = 1.0 / count;
        if(xScale.GetType() == scale::ScaleType::Linear || xScale.GetType() == scale::ScaleType::TimeSharingLinear) {
            normalizeSize *= (xScale.rangeMax - xScale.rangeMin);
        }
    }

    normalizeSize *= widthRatio;

    attr::AttrBase *attrBase = GetAttr(attr::AttrType::Adjust).get();
    if(attrBase != nullptr) {
        attr::Adjust *adjust = static_cast<attr::Adjust *>(attrBase);
        if(adjust->GetAdjust() == "dodge") {
            double size = fmax(dataArray_.size(), 1.);
            normalizeSize = normalizeSize / size;
        }
    }

    rst["x"] = x;
    rst["y0"] = y0;
    rst["size"] = normalizeSize;
    return rst;
}

nlohmann::json geom::Interval::getRectPoints(nlohmann::json &cfg) {
    double x = cfg["x"];
    auto y = cfg["y"];
    double y0 = cfg["y0"];
    double size = cfg["size"];

    double yMin = y0;
    double yMax;
    if(y.is_array()) {
        yMax = y[1];
        yMin = y[0];
    } else {
        yMax = y;
    }
    nlohmann::json rst;
    double xMin = x - size / 2;
    double xMax = x + size / 2;
    rst.push_back({{"x", xMin}, {"y", yMin}});
    rst.push_back({{"x", xMin}, {"y", yMax}});
    rst.push_back({{"x", xMax}, {"y", yMax}});
    rst.push_back({{"x", xMax}, {"y", yMin}});
    return rst;
}

void geom::Interval::BeforeMapping(XChart &chart, nlohmann::json &dataArray) {
    const std::string &yField = this->GetYScaleField();
    if(chart.GetCoord().GetType() == coord::CoordType::Polar) {
        shapeType_ = "sector";
    }
    for(int index = 0; index < dataArray.size(); ++index) {
        nlohmann::json &groupData = dataArray[index];
        for(int position = 0; position < groupData.size(); ++position) {
            nlohmann::json &item = groupData[position];

            if(!item.contains(yField)) {
                // 无效点
                continue;
            }
            nlohmann::json yValue = item[yField];
            nlohmann::json cfg = CreateShapePointsCfg(chart, item, index);
            nlohmann::json points = getRectPoints(cfg);
            item["_points"] = points;
            if(this->tagConfig_.is_object()) {
                item["_tag"] = tagConfig_;
                item["_tag"]["content"] = yValue.dump();
            }
            if(!this->styleConfig_.is_object()) {
                this->Style();
            }
            item["_style"] = styleConfig_;
            item["_style"]["content"] = yValue.dump();
        }
    }
}

void geom::Interval::Draw(XChart &chart, Group &container, const nlohmann::json &groupData) const {
    for(int i = 0; i < groupData.size(); ++i) {
        const nlohmann::json &item = groupData[i];
        chart.geomShapeFactory_->DrawGeomShape(chart, type_, shapeType_, item, container);
    }
}