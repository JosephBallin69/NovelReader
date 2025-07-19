#include "ErrorHandler.h"

void ErrorHandler::AddError(std::string errormessage) {

	Error error;
	error.errormessage = errormessage;
	error.index = errorlist.size();
	error.priority = 0;

	errorlist.push_back(error);
	std::cout << "Added error[" << error.index << "]: " << error.errormessage << std::endl;
}

std::vector <ErrorHandler::Error> ErrorHandler::GetErrors() {
	return errorlist;
}

void ErrorHandler::DisplayErrorMessagePopup(ErrorHandler::Error error) {
	const char* modalname = "Error: " + error.index;
	if (ImGui::BeginPopup(modalname)) {
		ImGui::Text(error.errormessage.c_str());
		ImGui::EndPopup();
	}
}

void ErrorHandler::ResolveError(ErrorHandler::Error error) {
	for (size_t i = 0; i < errorlist.size(); i++) {
		Error currenterror = errorlist[i];

		if (currenterror.index == error.index) {
			errorlist.erase(errorlist.begin() + i);
			std::cout << "Removed error: " << currenterror.index << std::endl;
		}

	}
}

ErrorHandler::ErrorHandler() {

}

ErrorHandler::~ErrorHandler() {
}