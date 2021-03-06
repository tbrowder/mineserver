/*
  Copyright (c) 2016, The Mineserver Project
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the The Mineserver Project nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>
#include <string>
#include "inventoryChest.h"
#include "user.h"
#include "mineserver.h"
#include "plugin.h"
#include "protocol.h"
#include "chunkmap.h"
#include "map.h"

bool InventoryChest::onwindowClick(User* user, int8_t windowID, int16_t slot, int8_t button, int16_t actionNumber,
                                  int16_t itemID, int8_t itemCount, int16_t itemUses, int8_t mode)
{

  if (windowID == WINDOW_LARGE_CHEST && slot > MAX_SLOT_LARGE_CHEST) return false;
  else if (windowID == WINDOW_CHEST && slot > MAX_SLOT_CHEST) return false;
  if (slot != -999 && slot < 0) return false;

  Inventory* inventory = ServerInstance->inventory();
  //Ack
  if(actionNumber)
  {
    // ToDo: actually check the action before ack
    user->writePacket(Protocol::confirmTransaction(windowID, actionNumber, 1));
  }

  // Handle drag mode in a base class helper function
  if (mode == INVENTORY_MODE_DRAG)
  {
    return this->handleDrag(user, windowID, slot, button, actionNumber, itemID, itemCount, itemUses, mode);
  }
  else if (mode == INVENTORY_MODE_NUMBER)
  {
    return this->handleNumber(user, windowID, slot, button, actionNumber, itemID, itemCount, itemUses, mode);
  }

  user->openInv.recordAction = false;

  //Click outside the window
  if (slot == -999)
  {
    //Dropping outside of the window
    if(button == 0 && mode == 0 && user->inventoryHolding.getType() != -1)
    {
      ServerInstance->map(user->pos.map)->createPickupSpawn((int)user->pos.x, (int)user->pos.y, (int)user->pos.z,
          user->inventoryHolding.getType(), user->inventoryHolding.getCount(),
          user->inventoryHolding.getHealth(), user);
      user->inventoryHolding.setType(-1);
      return true;
    }
    return true;
  }
  //on click-and-drag mode, recording the slots used
  else if(user->openInv.recordAction)
  {
    if(mode == 5)
    {
      user->openInv.slotActions.push_back(slot);
    }
    else
    {
      user->openInv.recordAction = false;
    }
    return true;
  }

  if (!user->isOpenInv && windowID != 0)
  {
    return false;
  }

  sChunk* chunk = NULL;
  chunk = ServerInstance->map(user->pos.map)->getChunk(blockToChunk(user->openInv.x), blockToChunk(user->openInv.z));

  if (chunk == NULL)
  {
    return false;
  }

  chunk->changed = true;

  chestDataPtr _thischest;
  for(size_t i = 0; i < chunk->chests.size(); i++)
  {
    if((chunk->chests[i]->x() == user->openInv.x)
      && (chunk->chests[i]->y() == user->openInv.y)
      && (chunk->chests[i]->z() == user->openInv.z))
    {
      _thischest = chunk->chests[i];
      break;
    }
  }

  std::vector<User*>* otherUsers = NULL;
  std::vector<User*>* otherUsersCon = NULL;

  std::vector<OpenInvPtr>& inv = inventory->openChests;

  for (size_t i = 0; i < inv.size(); i++)
  {
    if (inv[i]->x == user->openInv.x &&
        inv[i]->y == user->openInv.y &&
        inv[i]->z == user->openInv.z)
    {
      otherUsers = &inv[i]->users;
      break;
    }
  }
  // Users in connected chest
  if (_thischest->large())
  {
    for (size_t i = 0; i < inv.size(); i++)
    {
      if (inv[i]->x == _thischest->getConnectedLoc().x() &&
          inv[i]->y == _thischest->getConnectedLoc().y() &&
          inv[i]->z == _thischest->getConnectedLoc().z())
      {
        otherUsersCon = &inv[i]->users;
        break;
      }
    }
  }

  if (otherUsers == NULL)
  {
    return false;
  }

  Item* slotItem = NULL;

  switch (windowID)
  {
  case WINDOW_CHEST:
    if (slot > 26)
    {
      slotItem = &user->inv[slot - 18];
    }
    else
    {
      for (uint32_t i = 0; i < chunk->chests.size(); i ++)
      {
        if (chunk->chests[i]->x() == user->openInv.x &&
            chunk->chests[i]->y() == user->openInv.y &&
            chunk->chests[i]->z() == user->openInv.z)
        {
          slotItem = (*chunk->chests[i]->items())[slot].get();
          break;
        }
      }
      // Chest data has to exist at this point
      if (slotItem == NULL)
      {
        return false;
      }
    }
    break;
  case WINDOW_LARGE_CHEST:
    if (slot > 53)
    {
      slotItem = &user->inv[slot - 45];
    }
    else
    {
      // Search for this chest
      for (chestDataPtr chest : chunk->chests)
      {
        if(chest->x() == user->openInv.x &&
           chest->y() == user->openInv.y &&
           chest->z() == user->openInv.z)
        {
          // Chests are separate entities and connected with "top" bit telling which is the top part of inventory

          // Bottom part
          if (slot > 26) {
            slotItem = chest->getTop()?(*chest->getConnectedData()->items())[slot-27].get() : (*chest->items())[slot-27].get() ;
          } else { // Top part
            slotItem = chest->getTop()?(*chest->items())[slot].get() : (*chest->getConnectedData()->items())[slot].get();
          }
          break;
        }
      }

      // Chest data has to exist at this point
      if (slotItem == NULL)
      {
        return false;
      }

    }
    break;
  }
  
  //Empty slot and holding something
  if ((slotItem->getType() == -1 || (slotItem->getType() == user->inventoryHolding.getType() && slotItem->getHealth() == user->inventoryHolding.getHealth() && slotItem->getCount() < 64)) && user->inventoryHolding.getType() != -1)
  {
    //ToDo: Make sure we have room for the items!

    //Make sure not putting anything to the crafting space
    if ((windowID != WINDOW_CRAFTING_TABLE && windowID != WINDOW_PLAYER) || slot != 0)
    {
      int16_t addCount = (64 - slotItem->getCount() >= user->inventoryHolding.getCount()) ? user->inventoryHolding.getCount() : 64 - slotItem->getCount();

      slotItem->decCount(0 - ((button) ? 1 : addCount));
      slotItem->setHealth(user->inventoryHolding.getHealth());
      slotItem->setType(user->inventoryHolding.getType());

      user->inventoryHolding.decCount((button) ? 1 : addCount);
    }
  }
  //We are not holding anything, get the item we clicked
  else if (user->inventoryHolding.getType() == -1)
  {
    //Shift+click -> items to player inv
    //ToDo: from player inventory to chest
    if(!button && mode && inventory->isSpace(user, slotItem->getType(), slotItem->getCount()))
    {
      inventory->addItems(user, slotItem->getType(), slotItem->getCount(), slotItem->getHealth());
      slotItem->setCount(0);
    }
    else
    {
      user->inventoryHolding.setType(slotItem->getType());
      user->inventoryHolding.setHealth(slotItem->getHealth());
      user->inventoryHolding.setCount(slotItem->getCount());
      if (button == 1)
      {
        user->inventoryHolding.decCount(slotItem->getCount() >> 1);
      }
      slotItem->decCount(user->inventoryHolding.getCount());
    }
      
    if (slotItem->getCount() == 0)
    {
      slotItem->setHealth(0);
      slotItem->setType(-1);
    }
  }
  else
  {
    //Swap items if holding something and clicking another, not with craft slot
    int16_t type = slotItem->getType();
    int8_t count = slotItem->getCount();
    int16_t health = slotItem->getHealth();
    slotItem->setType(user->inventoryHolding.getType());
    slotItem->setCount(user->inventoryHolding.getCount());
    slotItem->setHealth(user->inventoryHolding.getHealth());
    user->inventoryHolding.setType(type);
    user->inventoryHolding.setCount(count);
    user->inventoryHolding.setHealth(health);
  }

  //Update slot
  inventory->setSlot(user, windowID, slot, slotItem);

  //Update item on the cursor
  inventory->setSlot(user, WINDOW_CURSOR, 0, &user->inventoryHolding);
  
  //Signal others using the same space
  switch(windowID)
  {
    case WINDOW_CHEST:
      if(slot > 26)
      {
        return true;
      }
      break;
    case WINDOW_LARGE_CHEST:
      if(slot > 53)
      {
        return true;
      }
  }

  for(uint32_t i = 0; i < otherUsers->size(); i++)
  {
    if((*otherUsers)[i] != user)
    {
      inventory->setSlot((*otherUsers)[i], windowID, slot, slotItem);
    }
  }

  // If connected chest inventory was found, loop that too
  if (otherUsersCon != NULL)
  {
    for(uint32_t i = 0; i < otherUsersCon->size(); i++)
    {
      if((*otherUsersCon)[i] != user)
      {
        inventory->setSlot((*otherUsersCon)[i], windowID, slot, slotItem);
      }
    } 
  }
  return true;
}

bool InventoryChest::onwindowOpen(User* user, int8_t type, int32_t x, int32_t y, int32_t z) const
{
  sChunk* chunk = ServerInstance->map(user->pos.map)->getChunk(blockToChunk(x), blockToChunk(z));
  if (chunk == NULL)
  {
    return false;
  }

  std::vector<OpenInvPtr>& inv = ServerInstance->inventory()->openChests;

  for (size_t i = 0; i < inv.size(); ++i)
  {
    if (inv[i]->x == x &&
        inv[i]->y == y &&
        inv[i]->z == z)
    {
      inv[i]->users.push_back(user);
      user->isOpenInv = true;
      user->openInv = *inv[i];
      break;
    }
  }

  if (!user->isOpenInv)
  {
    //If the inventory not yet opened, create it
    OpenInvPtr newInv(new OpenInventory());
    newInv->type = type;
    newInv->x    = x;
    newInv->y    = y;
    newInv->z    = z;
    user->openInv = *newInv;

    newInv->users.push_back(user);

    inv.push_back(newInv);
    user->isOpenInv = true;

    //Chest opening animation and sound
    user->sendAll(Protocol::blockAction(x,y,z,1,1,BLOCK_CHEST));
    user->sendAll(Protocol::namedSoundEffect("random.chestopen", x<<3, y<<3, z<<3, 1.0, 63));
  }

  // Get the actual data in the chest and dump it to the player
  chestDataPtr _chestData;
  for (uint32_t i = 0; i < chunk->chests.size(); i++)
  {
    if ((chunk->chests[i]->x() == x)
      && (chunk->chests[i]->y() == y)
      && (chunk->chests[i]->z() == z) )
    {
      _chestData = chunk->chests[i];
      break;
    }
  }
  if(_chestData != NULL)
  {
    std::string windowName = _chestData->large() ? "Large chest" : "Chest";

    user->writePacket(Protocol::openWindow(type,INVENTORYTYPE_CHEST,"{\"text\": \""+json_esc(windowName)+"\"}", _chestData->large()?54:27));

    uint32_t chestSlots = _chestData->size() * (_chestData->large() ? 2: 1);

    for (size_t j = 0; j < chestSlots; j++)
    {
      ItemPtr tmpItem;
      if (_chestData->large()) {
        if (j < 27) {
          tmpItem = _chestData->getTop() ? (*_chestData->items())[j] : (*_chestData->getConnectedData()->items())[j];
        } else {
          tmpItem = _chestData->getTop() ? (*_chestData->getConnectedData()->items())[j-27] : (*_chestData->items())[j-27];
        }
      }
      else
      {
        tmpItem = (*_chestData->items())[j];
      }

      // Send all non-empty slots
      if (tmpItem->getType() != -1)
      {
        user->writePacket(Protocol::setSlot(type, j, *tmpItem));
      }
    }
  }

  return true;
}

bool InventoryChest::onwindowClose(User* user, int8_t type, int32_t x, int32_t y, int32_t z) const
{
  std::vector<OpenInvPtr>& inv = ServerInstance->inventory()->openChests;

  for (size_t i = 0; i < inv.size(); ++i)
  {
    // Find this chest
    if (inv[i]->x == user->openInv.x &&
        inv[i]->y == user->openInv.y &&
        inv[i]->z == user->openInv.z)
    {
      // Find this user
      for (size_t j = 0; j < inv[i]->users.size(); ++j)
      {
        if (inv[i]->users[j] == user)
        {
          // We should make users into a container that supports fast erase.
          inv[i]->users.erase(inv[i]->users.begin() + j);

          if (inv[i]->users.empty())
          {
            user->sendAll(Protocol::blockAction(x,y,z,1,0,BLOCK_CHEST));
            user->sendAll(Protocol::namedSoundEffect("random.chestclosed", x<<3, y<<3, z<<3, 1.0, 63));
            inv.erase(inv.begin() + i);
          }

          user->isOpenInv = false;
          return true;
        }
      }
      break;
    }
  }

  user->isOpenInv = false;
  return true;
}