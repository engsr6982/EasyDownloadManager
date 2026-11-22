#include "CommandRegistry.h"

#include <algorithm>

namespace edm {

namespace {

/// 大小写不敏感的子序列匹配。返回匹配分（越大越相关），不匹配返回 -1。
int fuzzyScore(QStringView text, QStringView query) {
    if (query.isEmpty()) return 0;
    if (text.isEmpty()) return -1;

    int       score    = 0;
    int       ti       = 0;
    int       run      = 0;
    int       firstPos = -1;
    int const n        = text.size();
    int const m        = query.size();

    for (int qi = 0; qi < m; ++qi) {
        auto const qc  = query[qi].toLower();
        int        pos = -1;
        for (; ti < n; ++ti) {
            if (text[ti].toLower() == qc) {
                pos = ti++;
                break;
            }
        }
        if (pos < 0) return -1;
        if (firstPos < 0) firstPos = pos;

        if (run > 0 || (qi > 0 && pos > 0 && text[pos - 1].toLower() == query[qi - 1].toLower())) {
            ++run;
            score += 2 + run; // 连续命中加分
        } else {
            run    = 0;
            score += 1;
        }
    }

    if (firstPos == 0) score += 3;      // 前缀命中加分
    score += std::max(0, 4 - firstPos); // 首个命中越靠前分越高
    score -= n / 4;                     // 目标越长越不精确，轻微降权
    return score;
}

} // namespace

namespace {
CommandRegistry* g_registry{nullptr}; // create() 返回的引擎侧单例实例
} // namespace

CommandRegistry::CommandRegistry(QObject* parent) : QAbstractListModel(parent) { g_registry = this; }

CommandRegistry::~CommandRegistry() { g_registry = nullptr; }

CommandRegistry* CommandRegistry::create(QQmlEngine*, QJSEngine*) { return g_registry; }

std::int64_t CommandRegistry::addCommand(
    QString const&        source,
    QString const&        title,
    QString const&        description,
    std::function<void()> callback
) {
    auto const id = nextId_++;
    commands_.push_back({id, source, title, description, std::move(callback)});
    rebuildMatches();
    return id;
}

void CommandRegistry::removeCommand(std::int64_t id) {
    auto const before = commands_.size();
    std::erase_if(commands_, [id](Command const& cmd) { return cmd.id == id; });
    if (commands_.size() != before) rebuildMatches();
}

int CommandRegistry::rowCount(QModelIndex const& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(matches_.size());
}

QVariant CommandRegistry::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(matches_.size())) return {};
    auto const& cmd = commands_[static_cast<std::size_t>(matches_[static_cast<std::size_t>(index.row())].index)];
    switch (role) {
    case TitleRole:
        return cmd.title;
    case DescriptionRole:
        return cmd.description;
    case SourceRole:
        return cmd.source;
    default:
        return {};
    }
}

QHash<int, QByteArray> CommandRegistry::roleNames() const {
    return {
        {      TitleRole,       "title"},
        {DescriptionRole, "description"},
        {     SourceRole,      "source"},
    };
}

int CommandRegistry::count() const { return static_cast<int>(matches_.size()); }

void CommandRegistry::setFilter(QString const& text) {
    if (filter_ == text) return;
    filter_ = text;
    rebuildMatches();
}

void CommandRegistry::clearFilter() { setFilter(QString()); }

void CommandRegistry::executeRow(int row) {
    if (row < 0 || row >= static_cast<int>(matches_.size())) return;
    auto const& cmd      = commands_[static_cast<std::size_t>(matches_[static_cast<std::size_t>(row)].index)];
    auto        callback = cmd.callback; // 拷贝执行：脚本回调内可能自注销，引用会随容器收缩失效
    if (callback) callback();
}

void CommandRegistry::rebuildMatches() {
    beginResetModel();
    matches_.clear();
    QStringView const q(filter_);
    for (int i = 0; i < static_cast<int>(commands_.size()); ++i) {
        auto const& cmd = commands_[static_cast<std::size_t>(i)];
        if (int const s = fuzzyScore(cmd.title, q); s >= 0) {
            matches_.push_back({s + 10, i}); // 标题命中优先于描述命中
            continue;
        }
        if (int const s = fuzzyScore(cmd.description, q); s >= 0) {
            matches_.push_back({s, i});
        }
    }
    std::stable_sort(matches_.begin(), matches_.end(), [](Match const& a, Match const& b) {
        return a.score > b.score;
    });
    endResetModel();
    emit countChanged();
}

} // namespace edm
