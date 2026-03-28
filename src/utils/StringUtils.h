#pragma once
#include <QString>
#include <QStringView>
#include <string>
#include <string_view>


namespace edm::string_utils {


// ------------------------- QString -> 其他 -------------------------
inline std::string qstring2string(QString const& qs) { return qs.toStdString(); }

inline char* qstring2char(QString& qs) {
    return qs.toStdString().data(); // 非常量版本
}

inline QStringView qstring2qstringview(QString const& qs) { return QStringView(qs); }

// ------------------------- std::string -> 其他 -------------------------
inline QString string2qstring(std::string const& s) { return QString::fromStdString(s); }

inline std::string_view string2stringview(std::string const& s) { return std::string_view(s); }

inline char const* string2cstr(std::string const& s) { return s.c_str(); }

inline char* string2char(std::string& s) { return s.data(); }

// ------------------------- std::string_view -> 其他 -------------------------
inline std::string stringview2string(std::string_view sv) { return std::string(sv); }

inline QString stringview2qstring(std::string_view sv) { return QString::fromStdString(std::string(sv)); }

// ------------------------- char* / const char* -> 其他 -------------------------
inline QString cstr2qstring(char const* s) { return QString::fromUtf8(s); }

inline std::string cstr2string(char const* s) { return std::string(s); }

inline std::string_view cstr2stringview(char const* s) { return std::string_view(s); }

// ------------------------- QStringView -> 其他 -------------------------
inline QString qstringview2qstring(QStringView qsv) { return QString(qsv); }

inline std::string qstringview2string(QStringView qsv) { return qsv.toString().toStdString(); }

inline std::string_view qstringview2stringview(QStringView qsv) {
    return std::string_view(qsv.toString().toStdString());
}

inline char const* qstringview2cstr(QStringView qsv) { return qsv.toString().toStdString().c_str(); }

} // namespace edm::string_utils