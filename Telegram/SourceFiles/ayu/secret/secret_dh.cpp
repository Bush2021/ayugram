// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_dh.h"

namespace AyuSecret {

DhConfigCache::DhConfigCache(not_null<MTP::Sender*> api)
: _api(api) {
}

DhConfigCache::~DhConfigCache() {
	_api->request(base::take(_requestId)).cancel();
	Crypto::ZeroAndClear(_config.random);
}

const Crypto::DhConfig *DhConfigCache::cached() const {
	return _good ? &_config : nullptr;
}

void DhConfigCache::request(Callback done) {
	Expects(done != nullptr);

	_waiting.push_back(std::move(done));
	if (_requestId) {
		return;
	}
	_requestId = _api->request(MTPmessages_GetDhConfig(
		MTP_int(_good ? _config.version : 0),
		MTP_int(Crypto::kDhRandomSize)
	)).done([=](const MTPmessages_DhConfig &result) {
		_requestId = 0;
		apply(result);
	}).fail([=](const MTP::Error &error) {
		_requestId = 0;
		DEBUG_LOG(("Secret Error: getDhConfig failed, %1."
			).arg(error.type()));
		finish(false);
	}).send();
}

void DhConfigCache::apply(const MTPmessages_DhConfig &result) {
	const auto good = result.match([&](const MTPDmessages_dhConfig &data) {
		auto p = bytes::make_vector(data.vp().v);
		auto random = bytes::make_vector(data.vrandom().v);
		const auto g = data.vg().v;
		if (!Crypto::ValidateDhConfig(p, g)) {
			DEBUG_LOG(("Secret Error: the server sent an unusable group."));
			return false;
		} else if (int(random.size()) != Crypto::kDhRandomSize) {
			DEBUG_LOG(("Secret Error: the server sent %1 random bytes."
				).arg(int(random.size())));
			return false;
		}
		_config.p = std::move(p);
		_config.random = std::move(random);
		_config.g = g;
		_config.version = data.vversion().v;
		_good = true;
		return true;
	}, [&](const MTPDmessages_dhConfigNotModified &data) {
		auto random = bytes::make_vector(data.vrandom().v);
		if (!_good) {
			DEBUG_LOG(("Secret Error: the group is unchanged, "
				"but nothing was cached."));
			return false;
		} else if (int(random.size()) != Crypto::kDhRandomSize) {
			DEBUG_LOG(("Secret Error: the server sent %1 random bytes."
				).arg(int(random.size())));
			return false;
		}
		_config.random = std::move(random);
		return true;
	});
	finish(good);
}

void DhConfigCache::finish(bool good) {
	const auto config = good ? &_config : nullptr;
	for (const auto &callback : base::take(_waiting)) {
		callback(config);
	}
}

CreatorRequest MakeCreatorRequest(const Crypto::DhConfig &config) {
	auto a = Crypto::GenerateDhSecret(config.random);
	if (a.empty()) {
		return {};
	}
	auto gA = Crypto::ComputeGPow(config.p, config.g, a);
	if (gA.empty() || !Crypto::ValidateGPow(config.p, gA)) {
		Crypto::ZeroAndClear(a);
		return {};
	}
	return {
		.a = std::move(a),
		.gA = std::move(gA),
	};
}

AcceptorAnswer MakeAcceptorAnswer(
		const Crypto::DhConfig &config,
		bytes::const_span gA) {
	if (!Crypto::ValidateGPow(config.p, gA)) {
		return {};
	}
	auto b = Crypto::GenerateDhSecret(config.random);
	if (b.empty()) {
		return {};
	}
	const auto clearing = gsl::finally([&] {
		Crypto::ZeroAndClear(b);
	});
	auto key = Crypto::ComputeSharedKey(config.p, gA, b);
	if (key.empty()) {
		return {};
	}
	auto gB = Crypto::ComputeGPow(config.p, config.g, b);
	if (gB.empty() || !Crypto::ValidateGPow(config.p, gB)) {
		Crypto::ZeroAndClear(key);
		return {};
	}
	const auto fingerprint = Crypto::KeyFingerprint(key);
	return {
		.key = std::move(key),
		.gB = std::move(gB),
		.keyFingerprint = fingerprint,
	};
}

CreatorKey MakeCreatorKey(
		const Crypto::DhConfig &config,
		bytes::const_span gB,
		bytes::const_span a) {
	auto key = Crypto::ComputeSharedKey(config.p, gB, a);
	if (key.empty()) {
		return {};
	}
	const auto fingerprint = Crypto::KeyFingerprint(key);
	return {
		.key = std::move(key),
		.keyFingerprint = fingerprint,
	};
}

} // namespace AyuSecret
