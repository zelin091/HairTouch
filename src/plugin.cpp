// Anomaly 插件模板 —— UI 控件画廊（Widget Gallery）
//
// 本文件是一个可直接编译、加载的示例插件，逐一演示 Anomaly UI 服务
// （anomaly.ui）ABI v1 暴露的**全部**交互控件。它被刻意写成单文件、
// 强注释的形态，方便你 fork 后删掉不需要的段落、留下自己要用的控件。
//
// 设计要点：
//   * 只**必需**依赖 anomaly.ui 服务；本地化（anomaly.localization）是可选的。
//   * 运行期按 struct_size 逐字段探测能力（HAS 宏），因此在任何 >= V1 的
//     宿主上都能加载：宿主没有的新控件会被安全跳过，而不是崩溃。
//   * 所有 UI 调用只在 on_draw 回调内发生，符合 ABI 约定。
//
// 关于 ESP 绘制（draw_entity_bbox / draw_entity_box3d / draw_entity_label）：
// 它们是世界空间叠加绘制，需要一台相机与活动的游戏会话，并非窗口内控件，
// 因此不在本“控件画廊”演示范围内。可参考仓库内 EntityESP 插件。

#include "anomaly/sdk/cpp.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using anomaly::sdk::StringView;

// ---------------------------------------------------------------------------
// 能力探测：每次访问 UI 字段前，先用 struct_size 确认宿主确实实现到了该字段、
// 且函数指针非空，从而兼容 ABI v1 服务表的尾部扩展。这与官方 hello_ui 示例中的
// HAS_FIELD 思路一致。
// ---------------------------------------------------------------------------
#define HAS(ui, field)                                                        \
    ((ui) != nullptr &&                                                       \
     (ui)->struct_size >=                                                     \
         offsetof(AnomalyUiServiceV1, field) + sizeof((ui)->field) &&         \
     (ui)->field != nullptr)

// ImGui 条件/标志常量的稳定数值（宿主原样转交给 ImGui）。
constexpr std::uint32_t kCondFirstUseEver = 4u;   // ImGuiCond_FirstUseEver
constexpr std::uint32_t kTableFlags =             // 可缩放 + 行底色 + 完整边框
    (1u << 0) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9) | (1u << 10);

// on_draw 收到的 ui 可能为 null（宿主约定：此时使用 on_load 缓存的表）。
const AnomalyUiServiceV1* g_ui{};
const AnomalyLocalizationServiceV1* g_localization{};

// 画廊的全部可变状态都集中在这里，fork 时一目了然。
struct GalleryState {
    unsigned long long frames{};   // 由 on_update 累加，演示更新回调
    int button_clicks{};
    int toggle{1};
    int radio{0};                  // 用一组 checkbox 模拟单选
    float slider{0.35F};
    float color[4]{0.14F, 0.72F, 0.64F, 1.0F};
    std::uint32_t count{42};
    double coordinate{128.0};
    char name[64]{"Anomaly"};
    char filter[64]{};
    int show_modal{0};
} g_state;

// ---------------------------------------------------------------------------
// 本地化辅助：有 anomaly.localization 就翻译，否则回退到英文原文。
// ---------------------------------------------------------------------------
AnomalyStringViewV1 Tr(const char* key, const char* fallback, char* buffer,
                       std::size_t capacity) {
    if (g_localization != nullptr && g_localization->translate != nullptr &&
        buffer != nullptr && capacity != 0) {
        std::size_t size = capacity;
        const AnomalyStatusV1 status = g_localization->translate(
            g_localization->user, StringView(key), StringView(fallback), nullptr,
            0, buffer, &size);
        if (status.code == ANOMALY_STATUS_V1_OK && size >= 1) {
            return AnomalyStringViewV1{buffer, size - 1u};
        }
    }
    return StringView(fallback);
}

// 便捷包装：仅当宿主提供 text 时输出一行文本。
void Text(const AnomalyUiServiceV1* ui, const char* utf8) {
    if (HAS(ui, text)) ui->text(ui->user, StringView(utf8));
}

// 小标题：一条分隔线 + 一行文字，充当分组标题（宿主未暴露 CollapsingHeader）。
void SectionHeader(const AnomalyUiServiceV1* ui, const char* title) {
    if (HAS(ui, separator)) ui->separator(ui->user);
    Text(ui, title);
}

// 画廊里用于填充表格/过滤演示的控件清单。
struct WidgetInfo {
    const char* name;
    const char* category;
};
constexpr WidgetInfo kWidgets[] = {
    {"text", "\xE5\x9F\xBA\xE7\xA1\x80"},
    {"button", "\xE5\x9F\xBA\xE7\xA1\x80"},
    {"checkbox", "\xE5\x9F\xBA\xE7\xA1\x80"},
    {"slider_float", "\xE6\x95\xB0\xE5\x80\xBC"},
    {"color_edit4", "\xE6\x95\xB0\xE5\x80\xBC"},
    {"separator", "\xE5\xB8\x83\xE5\xB1\x80"},
    {"begin_child", "\xE5\xB8\x83\xE5\xB1\x80"},
    {"begin_table", "\xE5\xB8\x83\xE5\xB1\x80"},
    {"begin_menu", "\xE8\x8F\x9C\xE5\x8D\x95"},
    {"popup_modal", "\xE5\xBC\xB9\xE7\xAA\x97"},
    {"filter_match", "\xE8\xBE\x93\xE5\x85\xA5"},
    {"input_uint32", "\xE6\x95\xB0\xE5\x80\xBC"},
    {"input_double", "\xE6\x95\xB0\xE5\x80\xBC"},
    {"input_text", "\xE8\xBE\x93\xE5\x85\xA5"},
};

// ---------------------------------------------------------------------------
// 各分组的绘制函数
// ---------------------------------------------------------------------------

// 基础控件：文本、按钮、复选框、以及用复选框模拟的单选组。
void DrawBasics(const AnomalyUiServiceV1* ui) {
    SectionHeader(ui, "\xE2\x91\xA0 \xE5\x9F\xBA\xE7\xA1\x80\xE6\x8E\xA7\xE4\xBB\xB6");  // ① 基础控件

    char line[96];
    std::snprintf(line, sizeof(line), "button \xE7\x82\xB9\xE5\x87\xBB\xE6\xAC\xA1\xE6\x95\xB0: %d", g_state.button_clicks);
    Text(ui, line);
    if (HAS(ui, button) &&
        ui->button(ui->user, StringView("\xE7\x82\xB9\xE6\x88\x91 (button)"), 0.0F, 0.0F)) {
        ++g_state.button_clicks;
    }

    if (HAS(ui, checkbox)) {
        ui->checkbox(ui->user, StringView("\xE5\xBC\x80\xE5\x85\xB3 (checkbox)"), &g_state.toggle);
        // 用三个互斥复选框演示“单选组”模式。
        for (int i = 0; i < 3; ++i) {
            int selected = (g_state.radio == i) ? 1 : 0;
            char label[32];
            std::snprintf(label, sizeof(label), "\xE9\x80\x89\xE9\xA1\xB9 %d", i + 1);
            if (ui->checkbox(ui->user, StringView(label), &selected) && selected) {
                g_state.radio = i;
            }
        }
    }
}

// 数值控件：滑块、无符号整数、双精度、颜色选择器。
void DrawNumbers(const AnomalyUiServiceV1* ui) {
    SectionHeader(ui, "\xE2\x91\xA1 \xE6\x95\xB0\xE5\x80\xBC\xE4\xB8\x8E\xE9\xA2\x9C\xE8\x89\xB2");  // ② 数值与颜色

    if (HAS(ui, slider_float)) {
        ui->slider_float(ui->user, StringView("slider_float"), &g_state.slider, 0.0F, 1.0F);
    }
    if (HAS(ui, input_uint32)) {
        ui->input_uint32(ui->user, StringView("input_uint32"), &g_state.count, 1u, 10u);
    }
    if (HAS(ui, input_double)) {
        ui->input_double(ui->user, StringView("input_double"), &g_state.coordinate, 0.5, 10.0);
    }
    if (HAS(ui, color_edit4)) {
        ui->color_edit4(ui->user, StringView("color_edit4"), g_state.color);
        char rgba[96];
        std::snprintf(rgba, sizeof(rgba), "RGBA = %.2f, %.2f, %.2f, %.2f", g_state.color[0],
                      g_state.color[1], g_state.color[2], g_state.color[3]);
        Text(ui, rgba);
    }
}

// 输入 + 过滤 + 表格：input_text 输入过滤词，filter_match 过滤表格行。
void DrawInputAndTable(const AnomalyUiServiceV1* ui) {
    SectionHeader(ui, "\xE2\x91\xA2 \xE8\xBE\x93\xE5\x85\xA5 / \xE8\xBF\x87\xE6\xBB\xA4 / \xE8\xA1\xA8\xE6\xA0\xBC");  // ③ 输入/过滤/表格

    if (HAS(ui, input_text)) {
        ui->input_text(ui->user, StringView("\xE5\x90\x8D\xE7\xA7\xB0 (input_text)"), g_state.name,
                       sizeof(g_state.name), ANOMALY_UI_TEXT_INPUT_V1_NONE);
        char hello[128];
        std::snprintf(hello, sizeof(hello), "\xE4\xBD\xA0\xE5\xA5\xBD, %s!", g_state.name);
        Text(ui, hello);
    }
    if (HAS(ui, input_text)) {
        ui->input_text(ui->user, StringView("\xE8\xBF\x87\xE6\xBB\xA4 (filter)"), g_state.filter,
                       sizeof(g_state.filter), ANOMALY_UI_TEXT_INPUT_V1_NONE);
    }

    // 表格：列出所有控件；若填写了过滤词，用 filter_match 过滤行。
    if (HAS(ui, begin_table) && HAS(ui, table_next_row) && HAS(ui, table_next_column) &&
        HAS(ui, end_table) &&
        ui->begin_table(ui->user, StringView("widgets"), 3, kTableFlags, 0.0F, 160.0F)) {
        const AnomalyStringViewV1 filter = StringView(g_state.filter);
        const bool has_filter = HAS(ui, filter_match) && g_state.filter[0] != '\0';
        for (const WidgetInfo& widget : kWidgets) {
            if (has_filter &&
                ui->filter_match(ui->user, filter, StringView(widget.name)) == 0) {
                continue;
            }
            ui->table_next_row(ui->user);
            ui->table_next_column(ui->user);
            Text(ui, widget.name);
            ui->table_next_column(ui->user);
            Text(ui, widget.category);
            ui->table_next_column(ui->user);
            Text(ui, "V1");
        }
        ui->end_table(ui->user);
    }
}

// 布局容器：可滚动的子区域 begin_child / end_child。
void DrawChild(const AnomalyUiServiceV1* ui) {
    SectionHeader(ui, "\xE2\x91\xA3 \xE5\xAD\x90\xE5\x8C\xBA\xE5\x9F\x9F (begin_child)");  // ④ 子区域

    if (HAS(ui, begin_child) && HAS(ui, end_child) &&
        ui->begin_child(ui->user, StringView("scroll"), 0.0F, 90.0F, 0u)) {
        for (int i = 0; i < 20; ++i) {
            char line[48];
            std::snprintf(line, sizeof(line), "\xE5\x8F\xAF\xE6\xBB\x9A\xE5\x8A\xA8\xE8\xA1\x8C %d", i + 1);  // 可滚动行 N
            Text(ui, line);
        }
    }
    if (HAS(ui, end_child)) ui->end_child(ui->user);
}

// 弹窗与菜单：open_popup 打开模态框，框内用 begin_menu 演示子菜单。
void DrawPopupAndMenu(const AnomalyUiServiceV1* ui) {
    SectionHeader(ui, "\xE2\x91\xA4 \xE5\xBC\xB9\xE7\xAA\x97\xE4\xB8\x8E\xE8\x8F\x9C\xE5\x8D\x95");  // ⑤ 弹窗与菜单

    if (HAS(ui, button) && HAS(ui, open_popup) &&
        ui->button(ui->user, StringView("\xE6\x89\x93\xE5\xBC\x80\xE6\xA8\xA1\xE6\x80\x81\xE6\xA1\x86 (open_popup)"), 0.0F, 0.0F)) {
        ui->open_popup(ui->user, StringView("demo-modal"));
    }

    if (HAS(ui, begin_popup_modal) && HAS(ui, end_popup) &&
        ui->begin_popup_modal(ui->user, StringView("demo-modal"), nullptr, 0u)) {
        Text(ui, "\xE8\xBF\x99\xE6\x98\xAF\xE4\xB8\x80\xE4\xB8\xAA\xE6\xA8\xA1\xE6\x80\x81\xE5\xBC\xB9\xE7\xAA\x97\xE3\x80\x82");  // 这是一个模态弹窗。

        // 菜单（begin_menu / end_menu）在弹窗上下文中有效，演示为一个子菜单。
        if (HAS(ui, begin_menu) && HAS(ui, end_menu) &&
            ui->begin_menu(ui->user, StringView("\xE5\xAD\x90\xE8\x8F\x9C\xE5\x8D\x95 (begin_menu)"), 1)) {
            Text(ui, "\xE8\x8F\x9C\xE5\x8D\x95\xE9\xA1\xB9 A");
            Text(ui, "\xE8\x8F\x9C\xE5\x8D\x95\xE9\xA1\xB9 B");
            ui->end_menu(ui->user);
        }

        if (HAS(ui, button) &&
            ui->button(ui->user, StringView("\xE5\x85\xB3\xE9\x97\xAD (close)"), 0.0F, 0.0F) &&
            HAS(ui, close_current_popup)) {
            ui->close_current_popup(ui->user);
        }
        ui->end_popup(ui->user);
    }
}

// 元信息：帧状态、开发者模式、窗口尺寸——演示只读查询类接口。
void DrawMeta(const AnomalyUiServiceV1* ui) {
    SectionHeader(ui, "\xE2\x91\xA5 \xE5\x85\x83\xE4\xBF\xA1\xE6\x81\xAF");  // ⑥ 元信息

    char line[128];
    std::snprintf(line, sizeof(line), "on_update \xE5\xB8\xA7\xE8\xAE\xA1\xE6\x95\xB0: %llu", g_state.frames);
    Text(ui, line);

    if (HAS(ui, frame_state)) {
        const std::uint32_t s = ui->frame_state(ui->user);
        std::snprintf(line, sizeof(line), "frame_state: %s%s%s",
                      (s & ANOMALY_UI_FRAME_V1_ITEM_HOVERED) ? "hovered " : "",
                      (s & ANOMALY_UI_FRAME_V1_WINDOW_FOCUSED) ? "focused " : "",
                      (s & ANOMALY_UI_FRAME_V1_ITEM_ACTIVE) ? "active " : "");
        Text(ui, line);
    }
    if (HAS(ui, get_window_size)) {
        float w = 0.0F, h = 0.0F;
        ui->get_window_size(ui->user, &w, &h);
        std::snprintf(line, sizeof(line), "\xE7\xAA\x97\xE5\x8F\xA3\xE5\xB0\xBA\xE5\xAF\xB8: %.0f x %.0f", w, h);
        Text(ui, line);
    }
    if (HAS(ui, developer_mode_enabled) && ui->developer_mode_enabled(ui->user)) {
        Text(ui, "\xF0\x9F\x94\xA7 \xE5\xBC\x80\xE5\x8F\x91\xE8\x80\x85\xE6\xA8\xA1\xE5\xBC\x8F\xE5\xB7\xB2\xE5\x90\xAF\xE7\x94\xA8");  // 🔧 开发者模式已启用
    }
}

// ---------------------------------------------------------------------------
// 插件生命周期
// ---------------------------------------------------------------------------
AnomalyStatusV1 ANOMALY_CALL Load(const AnomalyHostApiV1* host, void** context) {
    if (context == nullptr) return {ANOMALY_STATUS_V1_INVALID_ARGUMENT, 0, {}};
    *context = nullptr;

    const anomaly::sdk::Host view(host);
    const auto ui = view.Query<AnomalyUiServiceV1>(
        ANOMALY_UI_SERVICE_V1_ID, ANOMALY_UI_SERVICE_V1_VERSION);
    if (!ui || !HAS(ui.get(), text)) {
        return {ANOMALY_STATUS_V1_UNAVAILABLE, 0, {}};
    }
    g_ui = ui.get();

    // 本地化是可选增强：拿不到就全程英文回退。
    const auto loc = view.Query<AnomalyLocalizationServiceV1>(
        ANOMALY_LOCALIZATION_SERVICE_V1_ID, ANOMALY_LOCALIZATION_SERVICE_V1_VERSION);
    g_localization = loc ? loc.get() : nullptr;

    g_state = GalleryState{};
    *context = &g_state;
    return anomaly::sdk::Ok();
}

AnomalyStatusV1 ANOMALY_CALL Start(void*) { return anomaly::sdk::Ok(); }
AnomalyStatusV1 ANOMALY_CALL Stop(void*, std::uint32_t) { return anomaly::sdk::Ok(); }

void ANOMALY_CALL Unload(void*) {
    g_ui = nullptr;
    g_localization = nullptr;
    g_state = GalleryState{};
}

void ANOMALY_CALL Update(void*, double) { ++g_state.frames; }

void ANOMALY_CALL Draw(void*, const AnomalyUiServiceV1* ui_v1) {
    const AnomalyUiServiceV1* ui = (ui_v1 != nullptr) ? ui_v1 : g_ui;
    if (!HAS(ui, begin_window) || !HAS(ui, end_window)) return;

    // 首次出现时给一个初始尺寸与最小尺寸约束，之后交给用户自由拖拽。
    if (HAS(ui, set_next_window_size_constraints)) {
        ui->set_next_window_size_constraints(ui->user, 360.0F, 320.0F, 100000.0F, 100000.0F);
    }
    if (HAS(ui, set_next_window_size)) {
        ui->set_next_window_size(ui->user, 460.0F, 560.0F, kCondFirstUseEver);
    }

    char title[96];
    // open 传 nullptr：画廊没有关闭按钮，始终可见，便于演示。
    if (ui->begin_window(ui->user, Tr("window.title", "Anomaly UI Gallery", title, sizeof(title)),
                         nullptr, 0u) != 0) {
        char greeting[192];
        ui->text(ui->user, Tr("greeting",
                              "\xE6\xAC\xA2\xE8\xBF\x8E\xE4\xBD\xBF\xE7\x94\xA8 Anomaly \xE6\x8F\x92\xE4\xBB\xB6\xE6\xA8\xA1\xE6\x9D\xBF\xE3\x80\x82",
                              greeting, sizeof(greeting)));
        DrawBasics(ui);
        DrawNumbers(ui);
        DrawInputAndTable(ui);
        DrawChild(ui);
        DrawPopupAndMenu(ui);
        DrawMeta(ui);
    }
    ui->end_window(ui->user);
}

}  // namespace

// ---------------------------------------------------------------------------
// 导出入口：宿主通过它拿到插件描述符（ABI v1）。
// ---------------------------------------------------------------------------
ANOMALY_SDK_EXPORT AnomalyStatusV1 ANOMALY_CALL AnomalyPluginEntryV1(
    AnomalyPluginDescriptorV1* descriptor) {
    if (descriptor == nullptr || descriptor->struct_size < sizeof(*descriptor)) {
        return {ANOMALY_STATUS_V1_INVALID_ARGUMENT, 0, {}};
    }
    *descriptor = {
        sizeof(*descriptor), ANOMALY_PLUGIN_API_V1_MAJOR, ANOMALY_PLUGIN_API_V1_MINOR,
        StringView("com.example.ui-gallery"), StringView("UI Gallery"),
        StringView("Your Name"), StringView("1.0.0"),
        Load, Start, Stop, Unload, Update, Draw};
    return anomaly::sdk::Ok();
}
