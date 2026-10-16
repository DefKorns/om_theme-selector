/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "download_screen.h"
#include "texture_utils.h"
#include "framework/draw_helpers.h"
#include "localization.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    constexpr int PollTimeoutMs = 20;
    constexpr Uint32 AutoCloseMs = 1500;
    constexpr int TitleSize = 28;
    constexpr int AuthorSize = 16;
    constexpr int DetailSize = 14;
    constexpr int MessageSize = 16;
    constexpr int BarW = 640;
    constexpr int BarH = 16;
    constexpr size_t MaxMessages = 4;

    class ChildProcess
    {
    public:
        explicit ChildProcess(const std::string & command)
        {
            int fds[2];
            if(pipe(fds) != 0)
                return;
            pid_ = fork();
            if(pid_ < 0)
            {
                close(fds[0]);
                close(fds[1]);
                return;
            }
            if(pid_ == 0)
            {
                setpgid(0, 0);
                dup2(fds[1], STDOUT_FILENO);
                close(fds[0]);
                close(fds[1]);
                execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
                _exit(127);
            }
            setpgid(pid_, pid_);
            close(fds[1]);
            out_ = fdopen(fds[0], "r");
            if(!out_)
                close(fds[0]);
        }

        ~ChildProcess() { Finish(true); }

        ChildProcess(const ChildProcess &) = delete;
        ChildProcess & operator=(const ChildProcess &) = delete;

        FILE * Output() const { return out_; }

        // exit status, or -1 when cancelled or not started
        int Finish(bool cancel)
        {
            if(pid_ <= 0)
                return -1;
            if(cancel)
                kill(-pid_, SIGTERM);
            if(out_)
                fclose(out_);
            out_ = nullptr;
            int status = 0;
            waitpid(pid_, &status, 0);
            pid_ = -1;
            return !cancel && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }

    private:
        pid_t pid_ = -1;
        FILE * out_ = nullptr;
    };

    // a trailing & would hand back the shell's status instead of the script's
    std::string Foreground(std::string command)
    {
        while(!command.empty() && (command.back() == ' ' || command.back() == '&'))
            command.pop_back();
        return command;
    }

    // "<label> <pct>% (...)" from the downloader; -1 when the size is unknown
    int ProgressPercent(const std::string & line)
    {
        const size_t sign = line.find("% (");
        if(sign == std::string::npos)
            return -1;
        size_t start = sign;
        while(start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9')
            --start;
        return start == sign ? -1 : std::min(100, std::atoi(line.c_str() + start));
    }

    bool IsBlank(const std::string & line)
    {
        return line.find_first_not_of(" \t") == std::string::npos;
    }
}

bool RunDownloadCommand(const std::string & command, const std::string & title, const std::string & author,
                        SDL_Context & sdlContext, Controller & controller, const ModernChrome & chrome)
{
    ChildProcess process(Foreground(command));
    FILE * out = process.Output();
    if(!out)
        return false;

    SDL_Renderer * renderer = sdlContext.renderer;
    const SDL_Rect frame = UiTheme::FrameRect;
    const int centerX = frame.x + frame.w / 2;
    const int contentW = frame.w - 2 * UiTheme::FrameInset;
    auto Centered = [&](const std::string & text, int size, Color color, int y)
    {
        Texture t(TruncateToWidth(text, size, contentW), size, renderer, 0, y, false, ToAbgr(color), true);
        t.rect.x = centerX - t.rect.w / 2;
        return t;
    };

    const int titleY = UiTheme::HeaderDividerY + 120;
    Texture titleText = Centered(Translate("INSTALLING_THEME") + " " + title, TitleSize, UiTheme::Text, titleY);
    Texture authorText;
    if(!author.empty())
        authorText = Centered("by: " + author, AuthorSize, UiTheme::TextDim, titleText.rect.y + titleText.rect.h + 8);
    const SDL_Rect track{ centerX - BarW / 2, titleY + 120, BarW, BarH };

    Texture exitLetter("B", 16, renderer, 0, 0, false, ToAbgr(UiTheme::BadgeLetter), true);
    Texture exitLabel(Translate("EXIT"), 16, renderer, 0, 0, false, ToAbgr(UiTheme::Text), true);
    const int badgeGroupW = UiTheme::BadgeOuterSize + UiTheme::BadgeLabelGap + exitLabel.rect.w;
    const SDL_Rect exitBadge{ UiTheme::BadgeClusterRightX - badgeGroupW, UiTheme::BadgeBandY, UiTheme::BadgeOuterSize, UiTheme::BadgeOuterSize };
    exitLetter.rect.x = exitBadge.x + (exitBadge.w - exitLetter.rect.w) / 2;
    exitLetter.rect.y = exitBadge.y + (exitBadge.h - exitLetter.rect.h) / 2;
    exitLabel.rect.x = exitBadge.x + exitBadge.w + UiTheme::BadgeLabelGap;
    exitLabel.rect.y = exitBadge.y + (exitBadge.h - exitLabel.rect.h) / 2;

    int percent = 0;
    Texture detailText;
    std::deque<std::string> messages;
    std::vector<Texture> messageTexts;
    auto ShowMessages = [&](size_t count)
    {
        messageTexts.clear();
        int y = track.y + BarH + 56;
        const size_t first = messages.size() > count ? messages.size() - count : 0;
        for(size_t i = first; i < messages.size(); ++i)
        {
            messageTexts.push_back(Centered(messages[i], MessageSize, UiTheme::Text, y));
            y += messageTexts.back().rect.h + 4;
        }
    };

    auto render = [&]()
    {
        sdlContext.StartFrame();
        DrawStrokeRect(renderer, UiTheme::OuterRect, UiTheme::Border, UiTheme::BorderWidth, UiTheme::BorderRadius);
        chrome.gearIcon.Draw(renderer);
        chrome.appTitleText.Draw(renderer);
        chrome.appVersionText.Draw(renderer);
        DrawHLine(renderer, UiTheme::HeaderDividerX, UiTheme::HeaderDividerX + UiTheme::HeaderDividerW, UiTheme::HeaderDividerY, UiTheme::Border, UiTheme::BorderWidth);
        DrawHLine(renderer, UiTheme::OuterRect.x, UiTheme::OuterRect.x + UiTheme::OuterRect.w, UiTheme::FooterDividerY, UiTheme::Border, UiTheme::BorderWidth);
        chrome.creditText.Draw(renderer);
        DrawRoundedFillRect(renderer, exitBadge, UiTheme::BadgeB, exitBadge.w / 2);
        exitLetter.Draw(renderer);
        exitLabel.Draw(renderer);

        titleText.Draw(renderer);
        authorText.Draw(renderer);
        DrawRoundedFillRect(renderer, track, UiTheme::Border, BarH / 2);
        const int fillW = track.w * percent / 100;
        if(fillW > 0)
            DrawRoundedFillRect(renderer, { track.x, track.y, std::max(fillW, BarH), BarH }, UiTheme::Accent, BarH / 2);
        detailText.Draw(renderer);
        for(Texture & t : messageTexts)
            t.Draw(renderer);
        SetDrawColor(renderer, UiTheme::Bg);
        sdlContext.EndFrame();
    };

    const int fd = fileno(out);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    char buffer[256] = {0};
    while(!feof(out))
    {
        struct pollfd pfd{ fd, POLLIN, 0 };
        poll(&pfd, 1, PollTimeoutMs);
        while(fgets(buffer, sizeof(buffer), out) != nullptr)
        {
            std::string line(buffer);
            const size_t newline = line.find('\n');
            if(newline != std::string::npos)
                line.erase(newline);
            if(!line.empty() && line[0] == '\r')
            {
                line.erase(0, 1);
                percent = std::max(0, ProgressPercent(line));
                detailText = Centered(line, DetailSize, UiTheme::TextDim, track.y + BarH + 12);
            }
            else if(!IsBlank(line))
            {
                messages.push_back(line);
                if(messages.size() > MaxMessages)
                    messages.pop_front();
            }
        }
        if(!feof(out))
            clearerr(out);

        controller.Update();
        if(controller.GetButtonStatus(B))
        {
            process.Finish(true);
            return false;
        }
        render();
    }

    const bool succeeded = process.Finish(false) == 0;
    if(succeeded)
        percent = 100;
    ShowMessages(succeeded ? 1 : MaxMessages);
    const Uint32 shownAt = SDL_GetTicks();
    for(;;)
    {
        controller.Update();
        if(controller.GetButtonStatus(B))
            break;
        if(succeeded && SDL_GetTicks() - shownAt >= AutoCloseMs)
            break;
        render();
    }
    return succeeded;
}
