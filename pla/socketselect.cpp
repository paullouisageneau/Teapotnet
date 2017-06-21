/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "pla/socketselect.hpp"
#include "pla/exception.hpp"

namespace pla
{

SocketSelect::SocketSelect(void) :
	mJoining(false)
{
	// Create thread
	mThread = std::thread([this]() {
		while(true)
		{
			fd_set readfds;
			int maxfd;
			{
				std::unique_lock<std::mutex> lock(mMutex);
				if(mJoining) break;
				
				// Fill fd set and compute max fd
				FD_ZERO(&readfds);
				maxfd = 0;
				for(const auto &p : mSockets)
				{
					FD_SET(p.first->mSock, &readfds);
					maxfd = std::max(maxfd, SOCK_TO_INT(p.first->mSock));
				}
			}

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 100000;	// 100ms
			int n = ::select(maxfd, &readfds, NULL, NULL, &tv);
			if (n < 0) throw Exception("Select failed on sockets");

			if(n)
			{
				std::unique_lock<std::mutex> lock(mMutex);

				for(auto &p : mSockets)
				{
					if(FD_ISSET(p.first->mSock, &readfds))
					{
						// Call reader
						p.second(p.first);
					}
				}
			}
		}
	});
}

SocketSelect::~SocketSelect(void)
{
	join();
}

void SocketSelect::add(Socket *sock, std::function<void(Socket*)> reader)
{
	std::unique_lock<std::mutex> lock(mMutex);
	Assert(sock);
	mSockets[sock] = std::move(reader);
}

void SocketSelect::remove(Socket *sock)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSockets.erase(sock);
}

void SocketSelect::clear(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSockets.clear();
}

void SocketSelect::join(void)
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mSockets.clear();
		mJoining = true;
	}
	
	mThread.join();
}

}
