#pragma once
#include "writing_app.hpp"
namespace s3app {
struct AppTestAccess {
 static editor::presentation::PresentationPort& activePresentation(App& app){return app.presentation_.active();}
 static editor::Document& document(App& app){return app.document_;}
 static editor::body_edit::BodyEditor& editor(App& app){return app.editor_;}
};
}
