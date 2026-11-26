#pragma once

#include "expected.h"

#include <QDebug>
#include <glaze/glaze.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace edm::ws {

/// 报文信封的探测视图：分发前只需要 id 与 method，其余字段跳过
struct PacketHeader {
    std::optional<std::int64_t> id;
    std::string                 method;
    std::string                 event;
};

/// 信封探测与 params 解析都允许额外字段
inline constexpr glz::opts kPacketOpts{.error_on_unknown_keys = false};

/// 请求体：params 缺省时按 payload 默认值处理，业务校验由 handler 负责
template <typename PayloadT>
struct RequestBody {
    std::optional<PayloadT> params;
};

/// 成功响应信封
template <typename ResultT>
struct ResponseEnvelope {
    std::int64_t id{0};
    bool         ok{true};
    ResultT      result;
};

/// 失败响应信封
struct ErrorEnvelope {
    std::int64_t id{0};
    bool         ok{false};
    std::string  error;
};

/// 事件推送信封
template <typename PayloadT>
struct EventEnvelope {
    std::string event;
    PayloadT    data;
};

/// 信封经 glaze 序列化为文本；失败时记录日志并返回空文本
template <typename EnvelopeT>
[[nodiscard]] std::string serializeEnvelope(EnvelopeT const& envelope) {
    std::string text;
    if (auto const ec = glz::write_json(envelope, text)) {
        qWarning().noquote() << "ws 信封序列化失败:" << glz::format_error(ec, text).c_str();
        return {};
    }
    return text;
}

/// 错误响应：没有 packet 实例时也能按信封 id 回复
[[nodiscard]] inline std::string serializeErrorEnvelope(std::int64_t id, std::string_view message) {
    return serializeEnvelope(ErrorEnvelope{.id = id, .error = std::string{message}});
}

/**
 * @brief 报文的类型擦除接口，分发路径只依赖此层。
 *
 * `deserialize` 从请求文本解出 params，`serializeError` 产出错误响应；
 * 结果序列化由模板层按自身 payload 类型实现。
 */
class IPacket {
public:
    virtual ~IPacket() = default;

    virtual void                       setId(std::int64_t id) = 0;
    [[nodiscard]] virtual std::int64_t id() const             = 0;

    /// 从请求文本反序列化 params，缺省时保留 payload 默认值
    virtual Expected<void> deserialize(std::string_view text) = 0;

    /// 序列化错误响应信封
    [[nodiscard]] virtual std::string serializeError(std::string_view message) const = 0;
};

/// 信封公共实现：id 存取与错误响应
class Packet : public IPacket {
public:
    void                       setId(std::int64_t id) final { id_ = id; }
    [[nodiscard]] std::int64_t id() const final { return id_; }

    [[nodiscard]] std::string serializeError(std::string_view message) const final {
        return serializeErrorEnvelope(id_, message);
    }

private:
    std::int64_t id_{0};
};

/**
 * @brief 混入 payload 的模板层：唯一接触反射的继承层。
 *
 * `deserialize` 把请求 params 解入自身 payload 字段，`serialize` 把结果对象
 * 序列化并包装为响应信封。
 */
template <typename PayloadT>
class WithPacketPayload : public PayloadT, public Packet {
public:
    using PayloadType = PayloadT;

    Expected<void> deserialize(std::string_view text) final {
        RequestBody<PayloadT> body;
        if (auto const ec = glz::read<kPacketOpts>(body, text)) {
            return makeStringError("params 解析失败: " + std::string(glz::format_error(ec, text)));
        }
        if (body.params) static_cast<PayloadT&>(*this) = std::move(*body.params);
        return {};
    }

    /// 结果包装为成功信封，序列化失败时返回错误信封
    template <typename ResultT>
    [[nodiscard]] std::string serialize(ResultT const& result) const {
        std::string text;
        if (auto const ec = glz::write_json(ResponseEnvelope<ResultT>{id(), true, result}, text)) {
            qWarning().noquote() << "ws 响应序列化失败:" << glz::format_error(ec, text).c_str();
            return serializeError("响应序列化失败");
        }
        return text;
    }
};

/// 事件数据包装为事件信封；序列化失败时记录日志并返回空文本
template <typename EventT>
[[nodiscard]] std::string serializeEvent(typename EventT::PayloadType const& data) {
    std::string text;
    EventEnvelope<typename EventT::PayloadType> const envelope{std::string{EventT::kEvent}, data};
    if (auto const ec = glz::write_json(envelope, text)) {
        qWarning().noquote() << "ws 事件序列化失败:" << glz::format_error(ec, text).c_str();
        return {};
    }
    return text;
}

} // namespace edm::ws