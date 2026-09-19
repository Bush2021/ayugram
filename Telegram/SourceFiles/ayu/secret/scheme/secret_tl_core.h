// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "mtproto/core_types.h"

// The end-to-end scheme is generated with the SecretTL / SecretTLD /
// secretc / secret_ prefixes so that none of its 44 names that also exist
// in api.tl collide with the MTP ones. The primitive types stay shared with
// MTProto, so a SecretTL buffer is an mtpBuffer and can be handed straight
// to the encryption helpers and to messages.sendEncrypted.

enum {
	secretc_int = tl::id_int,
	secretc_long = tl::id_long,
	secretc_double = tl::id_double,
	secretc_string = tl::id_string,
	secretc_bytes = tl::id_bytes,
	secretc_vector = tl::id_vector,
	secretc_flags = tl::id_flags,
};

using SecretTLint = MTPint;

inline SecretTLint secret_int(int32 v) {
	return MTP_int(v);
}

template <typename Flags>
using SecretTLflags = MTPflags<Flags>;

template <typename T>
inline SecretTLflags<base::flags<T>> secret_flags(base::flags<T> v) {
	return MTP_flags(v);
}

template <typename T, typename = std::enable_if_t<!std::is_same_v<T, int>>>
inline SecretTLflags<base::flags<T>> secret_flags(T v) {
	return MTP_flags(v);
}

inline tl::details::zero_flags_helper secret_flags(
		void(tl::details::zero_flags_helper::*value)()) {
	return MTP_flags(value);
}

using SecretTLlong = MTPlong;

inline SecretTLlong secret_long(uint64 v) {
	return MTP_long(v);
}

using SecretTLdouble = MTPdouble;

inline SecretTLdouble secret_double(float64 v) {
	return MTP_double(v);
}

using SecretTLstring = MTPstring;
using SecretTLbytes = MTPbytes;

inline SecretTLstring secret_string(const std::string &v) {
	return MTP_string(v);
}
inline SecretTLstring secret_string(const QString &v) {
	return MTP_string(v);
}
inline SecretTLstring secret_string(const char *v) {
	return MTP_string(v);
}
inline SecretTLstring secret_string() {
	return MTP_string();
}
SecretTLstring secret_string(const QByteArray &v) = delete;

inline SecretTLbytes secret_bytes(const QByteArray &v) {
	return MTP_bytes(v);
}
inline SecretTLbytes secret_bytes(QByteArray &&v) {
	return MTP_bytes(std::move(v));
}
inline SecretTLbytes secret_bytes() {
	return MTP_bytes();
}
inline SecretTLbytes secret_bytes(bytes::const_span buffer) {
	return MTP_bytes(buffer);
}
inline SecretTLbytes secret_bytes(const bytes::vector &buffer) {
	return MTP_bytes(buffer);
}

template <typename T>
using SecretTLvector = MTPvector<T>;

template <typename T>
inline SecretTLvector<T> secret_vector(uint32 count) {
	return MTP_vector<T>(count);
}
template <typename T>
inline SecretTLvector<T> secret_vector(uint32 count, const T &value) {
	return MTP_vector<T>(count, value);
}
template <typename T>
inline SecretTLvector<T> secret_vector(const QVector<T> &v) {
	return MTP_vector<T>(v);
}
template <typename T>
inline SecretTLvector<T> secret_vector(QVector<T> &&v) {
	return MTP_vector<T>(std::move(v));
}
template <typename T>
inline SecretTLvector<T> secret_vector() {
	return MTP_vector<T>();
}
