#include <stdexcept>

class Error : public std::exception {
public:
	Error(const std::string &msg) : msg(msg) {}

	const char *what() const noexcept override {
		return msg.c_str();
	}

private:
	std::string msg;
};
