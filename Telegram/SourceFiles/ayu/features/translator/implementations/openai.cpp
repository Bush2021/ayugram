// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/translator/implementations/openai.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/translator/html_parser.h"

#include <memory>
#include <optional>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Ayu::Translator {
namespace {

QString ApplyPromptPlaceholders(
		QString value,
		const QString &to,
		const int messageCount,
		const QString &messagesJson) {
	value.replace(QStringLiteral("{to}"), to);
	value.replace(QStringLiteral("{message_count}"), QString::number(messageCount));
	value.replace(QStringLiteral("{messages_json}"), messagesJson);
	return value;
}

QUrl NormalizeEndpoint(const QString &endpointText) {
	auto url = QUrl::fromUserInput(endpointText.trimmed());
	if (!url.isValid()) {
		return {};
	}
	const auto scheme = url.scheme().toLower();
	if ((scheme != QStringLiteral("http"))
		&& (scheme != QStringLiteral("https"))) {
		return {};
	}
	if (url.host().isEmpty()) {
		return {};
	}

	auto path = url.path();
	while ((path.size() > 1) && path.endsWith('/')) {
		path.chop(1);
	}
	if (path.isEmpty() || (path == QStringLiteral("/"))) {
		path = QStringLiteral("/v1/responses");
	} else if (path == QStringLiteral("/v1")) {
		path = QStringLiteral("/v1/responses");
	} else if (path != QStringLiteral("/v1/responses")) {
		return {};
	}
	url.setPath(path);
	return url;
}

struct StreamingResponseData {
	std::optional<QJsonObject> completedResponse;
	std::optional<QString> outputText;
	bool failed = false;
};

std::optional<StreamingResponseData> ParseStreamingResponse(const QByteArray &body) {
	auto text = QString::fromUtf8(body);
	text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	text.replace('\r', '\n');
	const auto trimmed = text.trimmed();
	if (!trimmed.startsWith(QStringLiteral("event:"))
		&& !trimmed.startsWith(QStringLiteral("data:"))) {
		return std::nullopt;
	}

	auto result = StreamingResponseData();
	auto accumulatedText = QString();
	const auto chunks = text.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
	for (const auto &chunk : chunks) {
		auto eventType = QString();
		auto dataLines = QStringList();
		for (const auto &line : chunk.split('\n')) {
			if (line.startsWith(QStringLiteral("event:"))) {
				eventType = line.mid(6).trimmed();
			} else if (line.startsWith(QStringLiteral("data:"))) {
				auto dataLine = line.mid(5);
				if (dataLine.startsWith(' ')) {
					dataLine.remove(0, 1);
				}
				dataLines.push_back(dataLine);
			}
		}
		if (dataLines.isEmpty()) {
			continue;
		}

		const auto data = dataLines.join('\n');
		const auto dataTrimmed = data.trimmed();
		if (dataTrimmed.isEmpty() || (dataTrimmed == QStringLiteral("[DONE]"))) {
			continue;
		}

		QJsonParseError parseError;
		const auto doc = QJsonDocument::fromJson(data.toUtf8(), &parseError);
		if ((parseError.error != QJsonParseError::NoError) || !doc.isObject()) {
			continue;
		}

		const auto eventObject = doc.object();
		const auto type = eventObject.value("type").toString(eventType);
		if (((type == QStringLiteral("response.completed"))
				|| (eventType == QStringLiteral("response.completed")))
			&& eventObject.value("response").isObject()) {
			result.completedResponse = eventObject.value("response").toObject();
			continue;
		}
		if (((type == QStringLiteral("response.failed"))
				|| (eventType == QStringLiteral("response.failed")))
			&& eventObject.value("response").isObject()) {
			result.failed = true;
			continue;
		}
		if (((type == QStringLiteral("error"))
				|| (eventType == QStringLiteral("error")))
			&& !eventObject.value("error").isUndefined()) {
			result.failed = true;
			continue;
		}
		if (((type == QStringLiteral("response.output_text.delta"))
				|| (eventType == QStringLiteral("response.output_text.delta")))
			&& eventObject.value("delta").isString()) {
			accumulatedText += eventObject.value("delta").toString();
			continue;
		}
		if (((type == QStringLiteral("response.output_text.done"))
				|| (eventType == QStringLiteral("response.output_text.done")))
			&& eventObject.value("text").isString()
			&& accumulatedText.isEmpty()) {
			accumulatedText = eventObject.value("text").toString();
		}
	}

	if (!accumulatedText.isEmpty()) {
		result.outputText = accumulatedText;
	}
	return result;
}

std::optional<std::vector<QString>> ParseTranslationsObject(
		const QJsonObject &object,
		int expectedCount) {
	const auto translationsValue = object.value("translations");
	if (!translationsValue.isArray()) {
		return std::nullopt;
	}

	const auto translationsArray = translationsValue.toArray();
	if (translationsArray.size() != expectedCount) {
		return std::nullopt;
	}

	auto result = std::vector<QString>();
	result.reserve(translationsArray.size());
	for (const auto &item : translationsArray) {
		if (!item.isString()) {
			return std::nullopt;
		}
		const auto text = item.toString();
		if (text.trimmed().isEmpty()) {
			return std::nullopt;
		}
		result.push_back(text);
	}
	return result;
}

std::optional<std::vector<QString>> ParseTranslationsValue(
		const QJsonValue &value,
		int expectedCount) {
	if (value.isObject()) {
		return ParseTranslationsObject(value.toObject(), expectedCount);
	}
	if (!value.isString()) {
		return std::nullopt;
	}

	QJsonParseError parseError;
	const auto doc = QJsonDocument::fromJson(
		value.toString().toUtf8(),
		&parseError);
	if ((parseError.error != QJsonParseError::NoError) || !doc.isObject()) {
		return std::nullopt;
	}
	return ParseTranslationsObject(doc.object(), expectedCount);
}

std::optional<std::vector<QString>> ParseResponseTranslations(
		const QJsonObject &root,
		int expectedCount) {
	if (const auto parsed = ParseTranslationsValue(
			root.value("output_parsed"),
			expectedCount)) {
		return parsed;
	}
	if (const auto parsed = ParseTranslationsValue(
			root.value("output_text"),
			expectedCount)) {
		return parsed;
	}
	const auto outputValue = root.value("output");
	if (!outputValue.isArray()) {
		return std::nullopt;
	}
	for (const auto &itemValue : outputValue.toArray()) {
		if (!itemValue.isObject()) {
			continue;
		}
		const auto item = itemValue.toObject();
		if (!item.value("content").isArray()) {
			continue;
		}
		for (const auto &contentValue : item.value("content").toArray()) {
			if (!contentValue.isObject()) {
				continue;
			}
			const auto content = contentValue.toObject();
			if (content.value("type").toString() != QStringLiteral("output_text")) {
				continue;
			}
			if (const auto parsed = ParseTranslationsValue(
					content.value("parsed"),
					expectedCount)) {
				return parsed;
			}
			if (const auto parsed = ParseTranslationsValue(
					content.value("text"),
					expectedCount)) {
				return parsed;
			}
		}
	}
	return std::nullopt;
}

bool HasResponseError(const QJsonObject &root) {
	const auto errorValue = root.value("error");
	return !errorValue.isUndefined() && !errorValue.isNull();
}

QString BuildMessagesJson(const std::vector<TextWithEntities> &texts) {
	auto messages = QJsonArray();
	for (const auto &text : texts) {
		messages.push_back(text.text);
	}
	return QString::fromUtf8(
		QJsonDocument(messages).toJson(QJsonDocument::Indented));
}

} // namespace

OpenAITranslator &OpenAITranslator::Instance() {
	static OpenAITranslator instance;
	return instance;
}

OpenAITranslator::OpenAITranslator(QObject *parent)
: BaseTranslator(parent) {
}

void OpenAITranslator::startTranslation(const StartTranslationArgs &args) {
	const auto &settings = AyuSettings::getInstance().openAiTranslationSettings();
	const auto &texts = args.parsedData.texts;
	const auto &toLang = args.parsedData.toLang;
	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;
	if (texts.empty() || toLang.trimmed().isEmpty()) {
		if (onFail) onFail();
		return;
	}

	const auto model = settings.model().trimmed().isEmpty()
		? OpenAiTranslationSettings::DefaultModel()
		: settings.model().trimmed();
	const auto endpointText = settings.apiBaseOrEndpoint().trimmed().isEmpty()
		? OpenAiTranslationSettings::DefaultApiBaseOrEndpoint()
		: settings.apiBaseOrEndpoint().trimmed();
	const auto endpoint = NormalizeEndpoint(endpointText);
	const auto apiKey = settings.apiKey().trimmed();
	if (!endpoint.isValid() || apiKey.isEmpty()) {
		if (onFail) onFail();
		return;
	}

	const auto to = toLang.trimmed();
	const auto messagesJson = BuildMessagesJson(texts);
	const auto instructions = ApplyPromptPlaceholders(
		settings.systemPrompt(),
		to,
		static_cast<int>(texts.size()),
		messagesJson);
	const auto promptTemplate = settings.promptTemplate().isEmpty()
		? OpenAiTranslationSettings::DefaultPromptTemplate()
		: settings.promptTemplate();
	const auto prompt = ApplyPromptPlaceholders(
		promptTemplate.trimmed().isEmpty()
		? OpenAiTranslationSettings::DefaultPromptTemplate()
		: promptTemplate,
		to,
		static_cast<int>(texts.size()),
		messagesJson);

	const auto schema = QJsonObject{
		{"type", "object"},
		{"properties", QJsonObject{
			{"translations", QJsonObject{
				{"type", "array"},
				{"items", QJsonObject{
					{"type", "string"}
				}},
				{"minItems", static_cast<int>(texts.size())},
				{"maxItems", static_cast<int>(texts.size())}
			}}
		}},
		{"required", QJsonArray{
			QStringLiteral("translations")
		}},
		{"additionalProperties", false}
	};
	const auto input = QJsonArray{
		QJsonObject{
			{"type", "message"},
			{"role", "user"},
			{"content", QJsonArray{
				QJsonObject{
					{"type", "input_text"},
					{"text", prompt}
				}
			}}
		}
	};
	auto bodyObject = QJsonObject{
		{"model", model},
		{"stream", false},
		{"input", input},
		{"text", QJsonObject{
			{"format", QJsonObject{
				{"type", "json_schema"},
				{"name", "translation_result"},
				{"strict", true},
				{"schema", schema}
			}}
		}}
	};
	if (!instructions.trimmed().isEmpty()) {
		bodyObject.insert("instructions", instructions);
	}
	const auto body = QJsonDocument(bodyObject).toJson(QJsonDocument::Compact);

	QNetworkRequest request(endpoint);
	request.setHeader(QNetworkRequest::UserAgentHeader, randomDesktopUserAgent());
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		QStringLiteral("application/json"));
	request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
	request.setRawHeader(
		QByteArrayLiteral("Authorization"),
		QByteArrayLiteral("Bearer ") + apiKey.toUtf8());

	QPointer<QNetworkReply> reply = _nam.post(request, body);

	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(15000);
	QObject::connect(
		timer,
		&QTimer::timeout,
		reply,
		[reply] {
			if (!reply) {
				return;
			}
			if (reply->isRunning()) {
				reply->abort();
			}
		});
	timer->start();

	QObject::connect(
		reply,
		&QNetworkReply::finished,
		reply,
		[reply, onSuccess = onSuccess, onFail = onFail, timer, expectedCount = static_cast<int>(texts.size())] {
			if (!reply) {
				return;
			}
			timer->stop();
			const auto guard = std::unique_ptr<QNetworkReply, void(*)(QNetworkReply*)>(
				reply,
				[](QNetworkReply *r) { r->deleteLater(); });
			const auto responseBody = reply->readAll();
			if (reply->error() != QNetworkReply::NoError) {
				if (onFail) onFail();
				return;
			}

			const auto produceSuccess = [&](const std::vector<QString> &translations) {
				auto result = std::vector<TextWithEntities>();
				result.reserve(translations.size());
				for (const auto &translation : translations) {
					result.push_back(shouldWrapInHtml()
						? Html::htmlToEntities(translation)
						: TextWithEntities{translation});
				}
				if (onSuccess) {
					onSuccess(result);
				}
			};

			if (const auto streamed = ParseStreamingResponse(responseBody)) {
				if (streamed->failed) {
					if (onFail) onFail();
					return;
				}
				if (streamed->outputText) {
					const auto translations = ParseTranslationsValue(
						QJsonValue(*streamed->outputText),
						expectedCount);
					if (translations) {
						produceSuccess(*translations);
						return;
					}
				}
				if (streamed->completedResponse) {
					if (HasResponseError(*streamed->completedResponse)) {
						if (onFail) onFail();
						return;
					}
					const auto translations = ParseResponseTranslations(
						*streamed->completedResponse,
						expectedCount);
					if (translations) {
						produceSuccess(*translations);
						return;
					}
				}
				if (onFail) onFail();
				return;
			}

			QJsonParseError parseError;
			const auto doc = QJsonDocument::fromJson(responseBody, &parseError);
			if ((parseError.error != QJsonParseError::NoError) || !doc.isObject()) {
				if (onFail) onFail();
				return;
			}

			const auto root = doc.object();
			if (HasResponseError(root)) {
				if (onFail) onFail();
				return;
			}

			const auto translations = ParseResponseTranslations(root, expectedCount);
			if (!translations) {
				if (onFail) onFail();
				return;
			}
			produceSuccess(*translations);
		});
}

} // namespace Ayu::Translator
