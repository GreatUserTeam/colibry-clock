// Clock: the reference Colibry OS app on Raven.
#include <colibry/app.hpp>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace {

std::string format(const char* pattern) {
	char buf[64];
	std::time_t now = std::time(nullptr);
	std::strftime(buf, sizeof(buf), pattern, std::localtime(&now));
	return buf;
}

}  // namespace

int main(int argc, char** argv) {
	const bool headless = argc > 1 && std::strcmp(argv[1], "--headless") == 0;
	if (!colibry::init({"Clock", headless})) return 1;

	raven::Window window = colibry::window_from_file(colibry::sharePath("clock/clock.xml"));
	if (!window.is_valid()) return 1;

	raven::Widget root(window.get_content());
	raven::Label time_label(root.find("time"));
	raven::Label date_label(root.find("date"));

	std::string last;
	while (raven::wait_frame(0.5)) {  // false once the window is closed or app.quit fired
		std::string now = format("%H:%M:%S");
		if (now != last) {
			last = now;
			time_label.set_text(now.c_str());
			date_label.set_text(format("%A, %d %B %Y").c_str());
		}
	}
	raven::shutdown();
	return 0;
}
