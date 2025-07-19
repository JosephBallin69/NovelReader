#pragma once
#include <string>
#include <vector>
#include "ImGui/imgui.h"
#include <iostream>

class ErrorHandler {
public:
	ErrorHandler();
	~ErrorHandler();

	struct Error {
		int index;
		int priority;
		std::string errormessage;
	};

	void AddError(std::string errormessage);
	void ResolveError(Error error);
	std::vector <Error> GetErrors();

	void DisplayErrorMessagePopup(Error error);

private:

	std::vector <Error> errorlist;

};

