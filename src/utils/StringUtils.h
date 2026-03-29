#pragma once
#include <QString>
#include <string>
#include <string_view>


namespace edm::string_utils {

inline std::string qstring2string(QString const& qs) { return qs.toStdString(); }

inline QString string2qstring(std::string const& s) { return QString::fromStdString(s); }

inline QString stringview2qstring(std::string_view sv) { return QString::fromStdString(std::string(sv)); }

} // namespace edm::string_utils