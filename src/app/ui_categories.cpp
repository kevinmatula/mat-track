// "Categories" dialog: add, rename, archive / unarchive, and delete unused
// categories. Archived ones disappear from the timer's dropdown but stay in
// the history and the filter.
#include <imgui.h>

#include <cstring>
#include <map>
#include <string>

#include "app/app.hpp"
#include "app/theme.hpp"

namespace mattrack {

namespace {

constexpr const char* kCategoriesDialog = "Categories";

}  // namespace

void App::draw_categories_dialog() {
  if (open_categories_dialog_) {
    ImGui::OpenPopup(kCategoriesDialog);
    open_categories_dialog_ = false;
    cat_selected_.reset();
    cat_name_.fill('\0');
    cat_message_.clear();
  }
  if (!ui::begin_dialog(kCategoriesDialog)) return;

  const float s = ui::scale();
  std::map<std::int64_t, int> counts;
  for (const auto& x : sessions_) ++counts[x.category_id];
  int active = 0;
  for (const auto& c : all_categories_) active += c.archived ? 0 : 1;

  // The list: click a row to select it (its name goes into the field).
  const float list_w = 460 * s;
  ui::begin_list("##categories", {{"Name", 0}, {"Sessions", 96 * s}, {"Status", 96 * s}},
                 {list_w, 200 * s});
  const ui::Palette& p = ui::palette();
  ImGui::PushStyleColor(ImGuiCol_Header, ImGui::ColorConvertU32ToFloat4(p.shadow));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::ColorConvertU32ToFloat4(p.light));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::ColorConvertU32ToFloat4(p.shadow));
  const float inset = 6 * s;
  for (const auto& c : all_categories_) {
    ImGui::PushID(static_cast<int>(c.id));
    const bool selected = cat_selected_ == c.id;
    ImGui::SetCursorPosX(0);
    if (ImGui::Selectable("##row", selected)) {
      // Clicking the selected row again deselects it (back to "Add").
      cat_name_.fill('\0');
      cat_message_.clear();
      if (selected) {
        cat_selected_.reset();
      } else {
        cat_selected_ = c.id;
        std::strncpy(cat_name_.data(), c.name.c_str(), cat_name_.size() - 1);
      }
    }
    ImGui::SameLine(inset);
    ImGui::TextUnformatted(c.name.c_str());
    ImGui::SameLine(ui::list_column_x(1) + inset);
    ImGui::Text("%d", counts[c.id]);
    ImGui::SameLine(ui::list_column_x(2) + inset);
    ImGui::TextUnformatted(c.archived ? "archived" : "active");
    ImGui::PopID();
  }
  ImGui::PopStyleColor(3);
  ui::end_list();

  // Find the selected category (it may have just been deleted).
  const Category* sel = nullptr;
  for (const auto& c : all_categories_) {
    if (cat_selected_ == c.id) sel = &c;
  }

  // Runs a change, then refreshes everything that shows categories.
  auto apply = [&](auto&& change, const std::string& done) {
    try {
      change();
      cat_message_ = done;
    } catch (const std::invalid_argument&) {
      cat_message_ = "Enter a name first.";
    } catch (const DbError&) {
      cat_message_ = "A category with that name already exists.";
    }
    reload_categories();
    reload_history();
  };
  const std::string name = cat_name_.data();

  ImGui::Spacing();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Name");
  ImGui::SameLine();
  const float buttons_w = 2 * 75 * s + 2 * ImGui::GetStyle().ItemSpacing.x;
  const bool enter = ui::input_text("##cat_name", cat_name_.data(), cat_name_.size(),
                                    ui::avail_width() - buttons_w);
  ImGui::SameLine();
  if (ui::button("Add", {}, true, !sel) || (enter && !sel)) {
    apply([&] { cat_selected_ = db_->add_category(name); }, "Added \"" + name + "\".");
  }
  ImGui::SameLine();
  if (ui::button("Rename", {}, sel != nullptr, sel != nullptr) || (enter && sel)) {
    if (sel) {
      const std::int64_t id = sel->id;
      apply([&] { db_->rename_category(id, name); }, "Renamed to \"" + name + "\".");
    }
  }

  // Re-find: the lambdas above may have reloaded all_categories_.
  sel = nullptr;
  for (const auto& c : all_categories_) {
    if (cat_selected_ == c.id) sel = &c;
  }

  ImGui::Spacing();
  if (sel && sel->archived) {
    if (ui::button("Unarchive")) {
      const std::int64_t id = sel->id;
      apply([&] { db_->set_category_archived(id, false); }, "Unarchived.");
    }
  } else {
    // Keep at least one category to time sessions with.
    const bool can_archive = sel && active > 1;
    if (ui::button("Archive", {}, can_archive) && sel) {
      const std::int64_t id = sel->id;
      apply([&] { db_->set_category_archived(id, true); }, "Archived. It stays in your history.");
    }
  }
  ImGui::SameLine();
  // Only unused categories can be deleted; the rest can be archived.
  const bool can_delete = sel && counts[sel->id] == 0 && (sel->archived || active > 1);
  if (ui::button("Delete", {}, can_delete) && sel) {
    const std::int64_t id = sel->id;
    apply(
        [&] {
          db_->delete_category(id);
          cat_selected_.reset();
          cat_name_.fill('\0');
        },
        "Deleted.");
  }
  ImGui::SameLine(list_w + ImGui::GetStyle().WindowPadding.x - 75 * s);
  if (ui::button("Close", {}, true, false)) ImGui::CloseCurrentPopup();

  // One line for feedback, or a hint about what is possible.
  ImGui::Spacing();
  if (!cat_message_.empty()) {
    ImGui::TextUnformatted(cat_message_.c_str());
  } else if (sel && counts[sel->id] > 0) {
    ImGui::TextDisabled("Categories with sessions can be archived, not deleted.");
  } else if (sel && !sel->archived && active == 1) {
    ImGui::TextDisabled("Keep at least one active category.");
  } else {
    ImGui::TextDisabled("Select a category to rename, archive or delete it.");
  }

  ui::end_dialog();
}

}  // namespace mattrack
