/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/board.hpp"
#include "tpn/resource.hpp"
#include "tpn/cache.hpp"
#include "tpn/html.hpp"
#include "tpn/user.hpp"
#include "tpn/store.hpp"
#include "tpn/config.hpp"

#include "pla/jsonserializer.hpp"
#include "pla/binaryserializer.hpp"
#include "pla/object.hpp"

namespace tpn
{

Board::Board(const String &name, const String &secret, const String &displayName) :
	mName(name),
	mDisplayName(displayName),
	mSecret(secret),
	mHasNew(false),
	mUnread(0)
{
	if(!mName.empty() && mName[0] == '/') mName = mName.substr(1);
	Assert(!mName.empty());

	Interface::Instance->add(urlPrefix(), this);

	const String prefix = "/mail/" + mName;

	Set<BinaryString> digests;
	Store::Instance->retrieveValue(Store::Hash(prefix), digests);

	for(auto it = digests.begin(); it != digests.end(); ++it)
	{
		//LogDebug("Board", "Retrieved digest: " + it->toString());
		if(fetch(Network::Link::Null, prefix, "/", *it, false))
			incoming(Network::Link::Null, prefix, "/", *it);
	}

	publish(prefix);
	subscribe(prefix);
}

Board::~Board(void)
{
	for(auto board : mSubBoards)
	{
		std::unique_lock<std::mutex> lock(board->mMutex);
		board->mBoards.erase(this);
	}

	Interface::Instance->remove(urlPrefix(), this);

	const String prefix = "/mail/" + mName;
	unpublish(prefix);
	unsubscribe(prefix);
}

String Board::urlPrefix(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return "/mail/" + mName;
}

bool Board::hasNew(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	bool value = false;
	std::swap(mHasNew, value);
	return value;
}

int Board::unread(void) const
{
	// TODO: count unread
	return 0;
}

void Board::addSubBoard(sptr<Board> board)
{
	std::unique_lock<std::mutex> lock1(mMutex, std::defer_lock);
	std::unique_lock<std::mutex> lock2(board->mMutex, std::defer_lock);
	std::lock(lock1, lock2);

	board->mBoards.insert(this);
	mSubBoards.insert(board);
	mMails.append(board->mMails);	// copy mails
}

void Board::removeSubBoard(sptr<Board> board)
{
	std::unique_lock<std::mutex> lock1(mMutex, std::defer_lock);
	std::unique_lock<std::mutex> lock2(board->mMutex, std::defer_lock);
	std::lock(lock1, lock2);

	board->mBoards.erase(this);
	mSubBoards.erase(board);
}

bool Board::post(const List<Mail> &mails)
{
	const String prefix = "/mail/" + mName;

	// Issue mails
	for(const Mail &m : mails)
		issue(prefix, m);

	// Add to chain
	return add(mails);
}

bool Board::post(const Mail &mail)
{
	List<Mail> tmp;
	tmp.push_back(mail);
	return post(tmp);
}

bool Board::add(const List<Mail> &mails)
{
	const String prefix = "/mail/" + mName;

	// First, append to list
	appendMails(mails);

	try {
		std::unique_lock<std::mutex> lock(mMutex);

		// Write messages to temporary file
		String tempFileName = File::TempName();
		File tempFile(tempFileName, File::Truncate);
		BinarySerializer serializer(&tempFile);
		for(const Mail &m : mails)
			serializer << m;
		tempFile.close();

		// Move to cache
		Resource resource;
		Resource::Specs specs;
		specs.name = mName;
		specs.type = "mail";
		specs.secret = mSecret;
		for(auto d : mDigests)
			specs.previousDigests.emplace_back(std::move(d));	// chain other messages
		resource.cache(tempFileName, specs);

		// Retrieve digest and store it
		BinaryString digest = resource.digest();
		Store::Instance->storeValue(Store::Hash(prefix), digest, Store::Permanent);

		// Update digests
		mPreviousDigests.insertAll(mDigests);
		mDigests.clear();
		mDigests.insert(digest);
		mProcessedDigests.insert(digest);

		// Republish prefix
		publish(prefix);
		return true;
	}
	catch(const Exception &e)
	{
		LogWarn("Board::process", String("Board post failed: ") + e.what());
		return false;
	}
}

void Board::appendMails(const List<Mail> &mails)
{
	std::unique_lock<std::mutex> lock(mMutex);

	for(const Mail &m : mails)
		mMails.emplace_back(std::move(m));

	// Notify HTTP clients
	mCondition.notify_all();

	for(auto b : mBoards)
		b->appendMails(mails);
}

bool Board::anounce(const Network::Link &link, const String &prefix, const String &path, List<BinaryString> &targets)
{
	std::unique_lock<std::mutex> lock(mMutex);
	targets.clear();
	for(auto d : mDigests)
		targets.emplace_back(std::move(d));
	return !targets.empty();
}

bool Board::incoming(const Network::Link &link, const String &prefix, const String &path, const BinaryString &target)
{
	if(!fetch(link, prefix, path, target, true))
		return false;

	try {
		std::unique_lock<std::mutex> lock(mMutex);

		if(mDigests.contains(target))
			return false;

		Resource resource(target, true);	// local only (already fetched)
		if(resource.type() != "mail")
			return false;

		if(!mPreviousDigests.contains(target))
			mDigests.insert(target);	// top-level

		// Fetch previous
		List<BinaryString> previous;
		if(resource.getPreviousDigests(previous))
			for(const BinaryString &d : previous)
			{
				mDigests.erase(d);
				mPreviousDigests.insert(d);
				fetch(link, prefix, path, d, true);
			}

		if(!mProcessedDigests.contains(target))
		{
			mProcessedDigests.insert(target);

			List<Mail> tmp;
			Resource::Reader reader(&resource, mSecret);
			BinarySerializer serializer(&reader);
			Mail m;
			while(serializer >> m)
			{
				if(m.empty()) continue;
				tmp.emplace_back(std::move(m));
			}

			lock.unlock();
			appendMails(tmp);
		}
	}
	catch(const Exception &e)
	{
		LogWarn("Board::incoming", e.what());
	}

	return true;
}

bool Board::incoming(const Network::Link &link, const String &prefix, const String &path, const Mail &mail)
{
	List<Mail> tmp;
	tmp.push_back(mail);
	return add(tmp);
}

void Board::http(const String &prefix, Http::Request &request)
{
	Assert(!request.url.empty());

	try {
		if(request.url == "/")
		{
			if(request.method == "POST")
			{
				if(request.post.contains("message") && !request.post["message"].empty())
				{
					Mail mail;
					mail.setContent(request.post["message"]);

					if(request.post.contains("parent"))
					{
						BinaryString parent;
						request.post["parent"].extract(parent);
						mail.setParent(parent);
					}

					if(request.post.contains("attachment"))
					{
						BinaryString attachment;
						request.post["attachment"].extract(attachment);
						if(!attachment.empty())
							mail.addAttachment(attachment);
					}

					if(request.post.contains("author"))
					{
						mail.setAuthor(request.post["author"]);
					}
					else {
						sptr<User> user = getAuthenticatedUser(request);
						if(user) {
							mail.setAuthor(user->name());
							mail.sign(user->identifier(), user->privateKey());
						}
					}

					post(mail);

					Http::Response response(request, 200);
					response.send();
				}

				throw 400;
			}

			if(request.get.contains("json"))
			{
				int next = 0;
				if(request.get.contains("next"))
					request.get["next"].extract(next);

				duration timeout = milliseconds(Config::Get("request_timeout").toInt());
				if(request.get.contains("timeout"))
					timeout = seconds(request.get["timeout"].toDouble());

				List<Mail*> tmp;
				{
					std::unique_lock<std::mutex> lock(mMutex);

					if(next >= int(mMails.size()))
					{
						mCondition.wait_for(lock, std::chrono::duration<double>(timeout), [this, next]() {
							return next < int(mMails.size());
						});
					}

					for(int i = next; i < int(mMails.size()); ++i)
						tmp.push_back(&mMails[i]);

					mUnread = 0;
					mHasNew = false;
				}

				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();

				JsonSerializer json(response.stream);
				json.setOptionalOutputMode(true);
				json << tmp;
				return;
			}

			bool isPopup = request.get.contains("popup");
			bool isFrame = request.get.contains("frame");

			Http::Response response(request, 200);
			response.send();

			Html page(response.stream);

			String title;
			{
				std::unique_lock<std::mutex> lock(mMutex);
				title = (!mDisplayName.empty() ? mDisplayName : "Board " + mName);
			}

			page.header(title, isPopup || isFrame);

			if(!isFrame)
			{
				page.open("div","topmenu");
				if(isPopup) page.span(title, ".button");

// TODO: should be hidden in CSS
#ifndef ANDROID
				if(!isPopup)
				{
					String popupUrl = Http::AppendParam(request.fullUrl, "popup");
					page.raw("<a class=\"button\" href=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','/');\">Popup</a>");
				}
#endif

				page.close("div");
			}

			page.open("div", ".replypanel");

			sptr<User> user = getAuthenticatedUser(request);
			if(user)
			{
				page.javascript("var TokenMail = '"+user->generateToken("mail")+"';\n\
						var TokenDirectory = '"+user->generateToken("directory")+"';\n\
						var TokenContact = '"+user->generateToken("contact")+"';\n\
						var UrlSelector = '"+user->urlPrefix()+"/myself/files/?json';\n\
						var UrlUpload = '"+user->urlPrefix()+"/files/_upload/?json';");

				page.raw("<a class=\"button\" href=\"#\" onclick=\"createFileSelector(UrlSelector, '#fileSelector', 'input.attachment', 'input.attachmentname', UrlUpload); return false;\"><img alt=\"File\" src=\"/static/paperclip.png\"></a>");
			}

			page.openForm("#", "post", "boardform");
			page.textarea("input");
			page.input("hidden", "attachment");
			page.input("hidden", "attachmentname");
			page.closeForm();
			page.close("div");
			page.div("","#attachedfile.attachedfile");
			page.div("", "#fileSelector.fileselector");

			if(isPopup) page.open("div", "board");
			else page.open("div", "board.box");

			page.open("div", "mail");
			page.open("p"); page.text("No messages"); page.close("p");
			page.close("div");

			page.close("div");

			page.javascript("function post() {\n\
					var message = $(document.boardform.input).val();\n\
					var attachment = $(document.boardform.attachment).val();\n\
					if(!message) return false;\n\
					var fields = {};\n\
					fields['message'] = message;\n\
					if(attachment) fields['attachment'] = attachment;\n\
					$.post('"+prefix+request.url+"', fields)\n\
						.fail(function(jqXHR, textStatus) {\n\
							alert('The message could not be sent.');\n\
						});\n\
					$(document.boardform.input).val('');\n\
					$(document.boardform.attachment).val('');\n\
					$(document.boardform.attachmentname).val('');\n\
					$('#attachedfile').hide();\n\
				}\n\
				$(document.boardform).submit(function() {\n\
					post();\n\
					return false;\n\
				});\n\
				$(document.boardform.attachment).change(function() {\n\
					$('#attachedfile').html('');\n\
					$('#attachedfile').hide();\n\
					var filename = $(document.boardform.attachmentname).val();\n\
					if(filename != '') {\n\
						$('#attachedfile').append('<img class=\"icon\" src=\"/static/file.png\">');\n\
						$('#attachedfile').append('<span class=\"filename\">'+filename+'</span>');\n\
						$('#attachedfile').show();\n\
					}\n\
					$(document.boardform.input).focus();\n\
					if($(document.boardform.input).val() == '') {\n\
						$(document.boardform.input).val(filename);\n\
						$(document.boardform.input).select();\n\
					}\n\
				});\n\
				$('#attachedfile').hide();");

			unsigned refreshPeriod = 2000;
			page.javascript("setMailReceiver('"+Http::AppendParam(request.fullUrl, "json")+"','#mail', "+String::number(refreshPeriod)+");");

			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("AddressBook::http", e.what());
		throw 500;
	}

	throw 404;
}

}
