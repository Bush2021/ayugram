// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {

class GenericBox;

void ExportSettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);
void ImportSettings(not_null<Window::SessionController*> controller);

} // namespace Ui
