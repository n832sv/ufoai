--!usr/bin/lua

--[[
-- @file
-- @brief Color configuration variables
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

do
	
	local colors = {

		1_verylight		=	{ 0.80, 0.96, 0.94, 1.00, },
		1_light			=	{},
		1_normal		=	{ 0.56, 0.81, 0.76, 1.00, },
		1_dark			=	{ 0.52, 0.76, 0.71, 1.00, },
		1_verydark		=	{ 0.05, 0.08, 0.07, 0.60, },

		2_verylight		=	{},
		2_light			=	{},
		2_normal		=	{},
		2_dark			=	{},
		2_verydark		=	{},

		3_verylight		=	{},
		3_light			=	{},
		3_normal		=	{ 1.0, 0.75, 0.00, 1.00, },
		3_dark			=	{},
		3_verydark		=	{},

	};

--		non grouped colors


--		methods

	colors.spread		= function (color) return table.unpack(colors[color]) end
	colors.str			= function (color) return table.concat(colors[color], " ") end
	
	return colors

end
