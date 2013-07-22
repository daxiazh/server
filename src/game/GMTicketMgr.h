/**
 * mangos-zero is a full featured server for World of Warcraft in its vanilla
 * version, supporting clients for patch 1.12.x.
 *
 * Copyright (C) 2005-2014  MaNGOS project <http://getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef _GMTICKETMGR_H
#define _GMTICKETMGR_H

#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Util.h"
#include "ObjectGuid.h"
#include "WorldSession.h"
#include "SharedDefines.h"
#include <map>

/**
 * \addtogroup game
 * @{
 * \file
 */

/**
 * This is the class that takes care of representing a ticket made to the GMs on the server
 * with a question of some sort.
 *
 * The code responsible for taking care of the opcodes coming
 * in can be found in:
 * - \ref WorldSession::SendGMTicketStatusUpdate
 * - \ref WorldSession::SendGMTicketGetTicket
 * - \ref WorldSession::HandleGMTicketGetTicketOpcode
 * - \ref WorldSession::HandleGMTicketUpdateTextOpcode
 * - \ref WorldSession::HandleGMTicketDeleteTicketOpcode
 * - \ref WorldSession::HandleGMTicketCreateOpcode
 * - \ref WorldSession::HandleGMTicketSystemStatusOpcode
 * - \ref WorldSession::HandleGMTicketSurveySubmitOpcode
 * These in their turn will make calls to the \ref GMTicketMgr which will take
 * care of what needs to be done by giving back a \ref GMTicket. The database table interesting
 * in this case is character_ticket in the characaters database.
 *
 * Theres also some handling of tickets in \ref ChatHandler::HandleTicketCommand where
 * you can turn on/off accepting tickets with your current GM char. You can also turn
 * off tickets globally, this will show the client a message about tickets not being
 * available at the moment. The commands that can be used are:
 * <dl>
 * <dt>.ticket on/off</dt>
 * <dd>Turns on/off showing new incoming tickets for you character</dd>
 * <dt>.ticket system_on/off</dt>
 * <dd>Will turn the whole ticket reporting system on/off, ie: if it's off the clients
 * will get a message that the system is unavailable when trying to submit a ticket</dd>
 * <dt>.ticket close $character_name/.ticket close #num_of_ticket</dt>
 * <dd>Will close a ticket for the given character name or the given number of the ticket,
 * this will make the little icon in the top right go away for the player</dd>
 * <dt>.ticket close_survey $character_name/.ticket close_survey #num_of_ticket</dt>
 * <dd>Does the same as .ticket close but instead of just closing it it also asks the \ref Player
 * to answer a survey about how please they were with the experience</dd>
 * <dt>.ticket respond $character_name/.ticket respond #num_of_ticket</dt>
 * <dd>Will respond to a ticket, this will whisper the \ref Player who asked the question and from
 * there on you will have to explain the solution etc. and then close the ticket again.</dd>
 * <dt>.ticket</dt>
 * <dd>Shows the number of currently active tickets</dd>
 * <dt>.ticket $character_name/.ticket #num_of_ticket</dt>
 * <dd>Will show the question and name of the character for the given ticket</dd>
 *
 * \todo Do not remove tickets from db when closing but mark them as solved instead.
 * \todo Log conversations between GM and the player receiving help.
 */
class GMTicket
{
    public:
        explicit GMTicket() : m_lastUpdate(0)
        {}

        /** 
         * Initializes this \ref GMTicket, much like the constructor would.
         * @param guid guid for the \ref Player that created the ticket
         * @param text the question text
         * @param responsetext the response to the question if any
         * @param update the last time the ticket was updated by either \ref Player or GM
         */
        void Init(ObjectGuid guid, const std::string& text, const std::string& responsetext, time_t update);

        /** 
         * Gets the \ref Player s \ref ObjectGuid which asked the question and created the ticket
         * @return the \ref ObjectGuid for the \ref Player that asked the question
         */
        ObjectGuid const& GetPlayerGuid() const { return m_guid; }
        /** 
         * Get the tickets question
         * @return the question this ticket had
         */
        const char* GetText() const { return m_text.c_str(); }
        /** 
         * Get the response given to this ticket, if any
         * @return the response that was made to this tickets question
         */
        const char* GetResponse() const { return m_responseText.c_str(); }
        /** 
         * Tells us when the last update was done as a UNIX timestamp.
         * @return Time since last update in seconds since UNIX epoch
         */
        uint64 GetLastUpdate() const { return m_lastUpdate; }

        /** 
         * Changes the tickets question text.
         * @param text the text to change the question to
         */
        void SetText(const char* text);
        /** 
         * Changes the response to the ticket
         * @param text the response to give
         * \deprecated
         */
        void SetResponseText(const char* text);

        /** 
         * Has this ticket gotten a response?
         * @return true if there's some kind of response to this ticket, false otherwise
         * \deprecated
         * \todo Change to resolved/not resolved instead, via the check in db
         */
        bool HasResponse() { return !m_responseText.empty(); };

        /**
         * This will delete this ticket from the characters database (table character_ticket)
         * and that in turn makes sure that the ticket isn't loaded as a new one when
         * restarting the server. 
         * \todo Mark the ticked as solved instead
         * \todo Log conversation between GM and player aswell
         */
        void DeleteFromDB() const;
        /** 
         * Saves the current state of this ticket to the DB in the characters database
         * in a table name character_ticket
         */
        void SaveToDB() const;
    
        /** 
         * This will take care of a \ref OpcodesList::CMSG_GMSURVEY_SUBMIT packet
         * and save the data received into the database, this is not implemented yet
         * @param recvData the packet we received with answers to the survey
         * \todo Implement saving this to DB
         */
        void SaveSurveyData(WorldPacket& recvData) const;
        /** 
         * Close this ticket so that the window showing in the client for the \ref Player
         * disappears.
         */
        void Close() const;
        /** 
         * This does the same thing as \ref GMTicket::Close but it also shows a survey window to the
         * \ref Player so that they can answer how well the GM behaved and such.
         * \todo Save the survey results in DB!
         */
        void CloseWithSurvey() const;
    private:
        void _Close(GMTicketStatus statusCode) const;
    
        ObjectGuid m_guid;
        std::string m_text;
        std::string m_responseText;
        time_t m_lastUpdate;
};
typedef std::map<ObjectGuid, GMTicket> GMTicketMap;
typedef std::list<GMTicket*> GMTicketList;                  // for creating order access

class GMTicketMgr
{
    public:
        //TODO: Make the default value a config option instead
        GMTicketMgr() : m_TicketSystemOn(true)
        {  }
        ~GMTicketMgr() {  }

        void LoadGMTickets();

        GMTicket* GetGMTicket(ObjectGuid guid)
        {
            GMTicketMap::iterator itr = m_GMTicketMap.find(guid);
            if (itr == m_GMTicketMap.end())
                { return NULL; }
            return &(itr->second);
        }

        size_t GetTicketCount() const
        {
            return m_GMTicketMap.size();
        }

        GMTicket* GetGMTicketByOrderPos(uint32 pos)
        {
            if (pos >= GetTicketCount())
                { return NULL; }

            GMTicketList::iterator itr = m_GMTicketListByCreatingOrder.begin();
            std::advance(itr, pos);
            if (itr == m_GMTicketListByCreatingOrder.end())
                { return NULL; }
            return *itr;
        }


        void Delete(ObjectGuid guid)
        {
            GMTicketMap::iterator itr = m_GMTicketMap.find(guid);
            if (itr == m_GMTicketMap.end())
                { return; }
            itr->second.DeleteFromDB();
            m_GMTicketListByCreatingOrder.remove(&itr->second);
            m_GMTicketMap.erase(itr);
        }

        void DeleteAll();

        void Create(ObjectGuid guid, const char* text)
        {
            GMTicket& ticket = m_GMTicketMap[guid];
            if (ticket.GetPlayerGuid())                     // overwrite ticket
            {
                ticket.DeleteFromDB();
                m_GMTicketListByCreatingOrder.remove(&ticket);
            }

            ticket.Init(guid, text, "", time(NULL));
            ticket.SaveToDB();
            m_GMTicketListByCreatingOrder.push_back(&ticket);
        }

       /** 
        * Turns on/off accepting tickets globally, if this is off the client will see a message
        * telling them that filing tickets is currently unavailable. When it's on anyone can
        * file a ticket.
        * @param accept true means that we accept tickets, false means that we don't
        */
        void SetAcceptTickets(bool accept) { m_TicketSystemOn = accept; };
        /** 
         * Checks if we accept tickets globally (see \ref GMTicketMgr::SetAcceptTickets)
         * @return true if we are accepting tickets globally, false otherwise
         * \todo Perhaps rename to IsAcceptingTickets?
         */
        bool WillAcceptTickets() { return m_TicketSystemOn; };
    private:
        bool m_TicketSystemOn;
        GMTicketMap m_GMTicketMap;
        GMTicketList m_GMTicketListByCreatingOrder;
};

#define sTicketMgr MaNGOS::Singleton<GMTicketMgr>::Instance()

/** @} */
#endif
