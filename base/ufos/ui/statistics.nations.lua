--!usr/bin/lua

--[[
-- @file
-- @brief Nation statistics widget for statistics screen
--]]

--[[
Copyright (C) 2002-2019 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
--]]

require("ufox.lua")
require("ufox.datatable.lua")

function build_nationstats (rootNode)
	if (rootNode == nil) then
		return
	end

	if (rootNode:child("nationslist") ~= nil) then
		rootNode:child("nationslist"):delete_node()
	end

	local nationStat = ufox.datatable.build(rootNode, {
		name = "nationslist",
		pos = {15, 41},
		size = {994, 420},
	})

	nationStat:add_headerFields({
		{ name = "flag",        width =  24, ghost = true, },
		{ name = "nation_name", width = 290, text = "_Nation", },
		{ name = "funding",     width = 108, text = "_Funding",   contentalign = ufo.ALIGN_CR, },
		{ name = "happiness",   width = 108, text = "_Approval", contentalign = ufo.ALIGN_CR, },
		{ name = "population",  width = 108, text = "_Population", contentalign = ufo.ALIGN_CR, },
		{ name = "soldiers",    width = 24,	 source = "added/tinyhead_soldier.png", 	class = "image", keepratio = true, ghost = true, contentalign = ufo.ALIGN_CR, },
		{ name = "scientists",  width = 24,	 source = "added/tinyhead_scientist.png", 	class = "image", keepratio = true, ghost = true, contentalign = ufo.ALIGN_CR, },
		{ name = "workers",     width = 24,	 source = "added/tinyhead_worker.png", 		class = "image", keepratio = true, ghost = true, contentalign = ufo.ALIGN_CR, },
		{ name = "pilots",      width = 24,	 source = "added/tinyhead_pilot.png", 		class = "image", keepratio = true, ghost = true, contentalign = ufo.ALIGN_CR, },
		{ name = "gdp",   		width = 120, text = "_GDP", contentalign = ufo.ALIGN_CR, },
		{ name = "gdpc",   		width = 120, text = "_per Capita", contentalign = ufo.ALIGN_CR, },
	})

	if (rootNode:child("ui_nations_fill") ~= nil) then
		rootNode:child("ui_nations_fill"):delete_node()
	end
	local nations_add = ufox.build({
		name = "ui_nations_fill",
		class = "confunc",

		on_click = function (sender, nationID, nationName, monthIDX, nationHappiness, nationHappinessString, nationFunding, population,  gdp, gdpc, maxSoldiers, maxScientists, maxWorkers, maxPilots, nationColor)
			if (monthIDX ~= "0") then
				return
			end

			-- ufo.print(string.format("Got data for nation %s month %s\n", nationID, monthIDX))
			local row = sender:parent():child("nationslist"):add_dataRow({name = nationID}, {
				{ name = "flag",        source = "nations/" .. nationID .. "_small", class = "image", keepratio = true, ghost = true, },
				{ name = "nation_name", text = nationName, },
				{ name = "funding",     text = string.format("_%d c", nationFunding), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = nationFunding, invisible = true, },
				},
				{ name = "happiness",   text = nationHappinessString, contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = nationHappiness, invisible = true, },
				},
				{ name = "population",     text = string.format("%d M", population), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = population, invisible = true, },
				},
				{ name = "soldiers",    text = string.format("%d", maxSoldiers), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = maxSoldiers, invisible = true, },
				},				
				{ name = "scientists",	text = string.format("%d", maxScientists), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = maxScientists, invisible = true, },
				},
				{ name = "workers",     text = string.format("%d", maxWorkers), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = maxWorkers, invisible = true, },
				},
				{ name = "pilots",     	text = string.format("%d", maxPilots), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = maxPilots, invisible = true, },
				},
				{ name = "gdp",     text = string.format("%d Mc", gdp), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = gdp, invisible = true, },
				},
				{ name = "gdpc",     text = string.format("%d c", gdpc), contentalign = ufo.ALIGN_CR,
					{ name = "value", class = "string", text = gdpc, invisible = true, },
				},

			})
		end,
	}, rootNode);

	return nationStat
end
