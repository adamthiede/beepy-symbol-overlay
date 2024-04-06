#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/ioctl.h>

#include <stdexcept>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "Overlay.hpp"

#include <SDL2/SDL.h>

using namespace std::literals;

struct OverlayStorage
{
	int x, y;
	size_t width, height;
	std::unique_ptr<unsigned char[]> pix;
};

using UniqueWindow = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
using UniqueRenderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

static std::unordered_map<int, UniqueWindow> g_windows;
static std::unordered_map<int, UniqueRenderer> g_renderers;
static std::unordered_map<void*, OverlayStorage> g_overlayStorages;

SharpSession::SharpSession(char const* sharp_dev)
	: m_fd{(int)g_windows.size()}
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		throw std::runtime_error(SDL_GetError());
	}

	auto window = UniqueWindow{
		SDL_CreateWindow("Overlay Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			400, 240, SDL_WINDOW_SHOWN),
		SDL_DestroyWindow};
	if (window == nullptr) {
		throw std::runtime_error(SDL_GetError());
	}

	auto renderer = UniqueRenderer{
		SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED),
		SDL_DestroyRenderer};
	if (renderer == nullptr) {
		throw std::runtime_error(SDL_GetError());
	}

	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
	SDL_RenderClear(renderer.get());
	SDL_RenderPresent(renderer.get());

	g_windows.emplace(m_fd, std::move(window));
	g_renderers.emplace(m_fd, std::move(renderer));

}

SharpSession::SharpSession(SharpSession&& expiring)
	: m_fd{expiring.m_fd}
{
	expiring.m_fd = -1;
}

SharpSession::~SharpSession()
{
	auto quit = false;
	auto event = SDL_Event{};
	while (!quit) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				quit = true;
			}
		}
	}

	if (m_fd >= 0) {
		g_renderers.erase(m_fd);
		g_windows.erase(m_fd);
	}

	if (g_windows.empty() && g_renderers.empty()) {
		SDL_Quit();
	}
}

int SharpSession::get()
{
	return m_fd;
}

Overlay::Overlay(SharpSession& session,
	int x, int y, size_t width, size_t height, unsigned char const* pix_)
	: m_session{session}
	, m_storage{(void*)this}
	, m_display{}
{
	if (x < 0) { x = 400 + x; }
	if (y < 0) { y = 240 + y; }

	auto pix = std::make_unique<unsigned char[]>(height * width);
	std::memcpy(pix.get(), pix_, height * width);
	g_overlayStorages.emplace(m_storage, OverlayStorage{
		x, y, width, height, std::move(pix)});
}

Overlay::Overlay(Overlay&& expiring)
	: m_session{expiring.m_session}
	, m_storage{expiring.m_storage}
	, m_display{expiring.m_display}
{
	expiring.m_storage = nullptr;
	expiring.m_display = nullptr;
}

Overlay::~Overlay()
{
	g_overlayStorages.erase(m_storage);
}

void Overlay::show()
{
	auto renderer = g_renderers.at(m_session.get()).get();
	auto& os = g_overlayStorages.at(m_storage);

	for (auto y = os.y; y < os.y + os.height; y++) {
		for (auto x = os.x; x < os.x + os.width; x++) {

			auto px = (os.pix.get()[((y - os.y) * os.width) + (x - os.x)])
				? 255
				: 0;
			SDL_SetRenderDrawColor(renderer, px, px, px, 255);
			SDL_RenderDrawPoint(renderer, x, y);
		}
	}

	SDL_RenderPresent(renderer);
}

void Overlay::hide()
{}

void Overlay::eject()
{}


void Overlay::clear_all(SharpSession& session)
{}
