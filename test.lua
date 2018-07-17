require( "openvr" )

local w, h
local aspect
local fov
local rt

local mat = CreateMaterial( "vr", "UnlitGeneric", {
	[ "$basetexture" ] = "vr"
} ) 

local function debugprint( str, x, y )
	draw.SimpleTextOutlined( str, "DebugFixed", x, y, color_white, TEXT_ALIGN_LEFT, TEXT_ALIGN_BOTTOM, 1, color_black )
end

RunConsoleCommand( "gmod_mcore_test", 0 )

local RENDERING_VR = false

local left_hand, right_hand

hook.Add( "HUDPaint", "Test", function()
	if not rt then return end

	surface.SetDrawColor( color_white )
	surface.SetMaterial( mat )
	surface.DrawTexturedRectUV( 0, 0, ScrW(), ScrH(), 0, 0, 0.5, 1 )
end )

hook.Add( "CreateMove", "VRInput", function( cmd )
	local head_pose = vr.GetDevicePose( vr.GetDeviceIndex( 1 ) )

	local left_state = vr.GetControllerState( vr.GetDeviceIndex( 2 ) )
	if not left_state then return end

	local right_state = vr.GetControllerState( vr.GetDeviceIndex( 3 ) )
	if not right_state then return end

	local left_stick = left_state.axes[ 1 ]
	local left_trigger = left_state.pressed[ 34 ]
	local left_grip = left_state.pressed[ 3 ]
	local left_click = left_state.pressed[ 33 ]
	local x = left_state.pressed[ 8 ]
	local y = left_state.pressed[ 2 ]

	local right_stick = right_state.axes[ 1 ]
	local right_trigger = right_state.pressed[ 34 ]
	local right_grip = right_state.pressed[ 3 ]
	local right_click = right_state.pressed[ 33 ]
	local a = right_state.pressed[ 8 ]
	local b = right_state.pressed[ 2 ]

	cmd:SetViewAngles( head_pose.ang )
	cmd:SetSideMove( left_stick.x * LocalPlayer():GetRunSpeed() )
	cmd:SetForwardMove( left_stick.y * LocalPlayer():GetRunSpeed() )

	local buttons = cmd:GetButtons()

	if right_trigger then buttons = bit.bor( buttons, IN_ATTACK ) end
	if right_grip then buttons = bit.bor( buttons, IN_ALT1 ) end
	if right_click then buttons = bit.bor( buttons, IN_RELOAD ) end

	if left_trigger then buttons = bit.bor( buttons, IN_ATTACK2 ) end
	if left_grip then buttons = bit.bor( buttons, IN_ALT2 ) end
	if left_click then buttons = bit.bor( buttons, IN_JUMP ) end

	cmd:SetButtons( buttons )
end )

hook.Add( "Think", "VRHands", function()
	vr.Update()

	if not left_hand then
		left_hand = ClientsideModel( "models/props_junk/PopCan01a.mdl" )
	end

	local left_pose = vr.GetDevicePose( vr.GetDeviceIndex( 2 ) )
	left_hand:SetPos( LocalPlayer():GetPos() + left_pose.pos )
	left_hand:SetAngles( left_pose.ang )

	if not right_hand then
		right_hand = ClientsideModel( "models/props_junk/PopCan01a.mdl" )
	end

	local right_pose = vr.GetDevicePose( vr.GetDeviceIndex( 3 ) )
	right_hand:SetPos( LocalPlayer():GetPos() + right_pose.pos )
	right_hand:SetAngles( right_pose.ang )

end )

hook.Add( "RenderScene", "RenderVR", function()
	if RENDERING_VR then return end

	if not vr.Ready() then return end

	if not rt then
		w, h, aspect, fov = vr.GetViewParameters()

		vr.CaptureTexture()
		rt = GetRenderTarget( "vr", w, h, false )
	end

	if not rt then return end

	local pose = vr.GetDevicePose( 0 )
	local pos = pose.pos
	local ang = pose.ang

	pos = pos + LocalPlayer():GetPos()

	--]]
	--pose.ang.y = -pose.ang.y

	--local ang = forward:Angle()

	--debugprint( "Pose (position): " .. tostring( pose.pos ), 10, 10 )
	--debugprint( "Pose (angle): " .. tostring( pose.ang ), 10, 20 )
	--debugprint( "Pose (velocity): " .. tostring( pose.vel ), 10, 30 )
	--debugprint( "Pose (angular velocity): " .. tostring( pose.angvel ), 10, 40 )
	--debugprint( "Eye position: " .. tostring( EyePos() ), 10, 50 )
	--debugprint( "Eye angles:  " .. tostring( EyeAngles() ), 10, 60 )
	--debugprint( "Fixed pose (position): " .. tostring( pos ), 10, 70 )
	--debugprint( "Fixed pose (angle): " .. tostring( ang ), 10, 80 )

	for i = 1, 4 do
		--debugprint( "{ " .. table.concat( pose.pose[ i ], ", " ) .. " }", 10, 80 + i * 10 ) 
	end

	--debugoverlay.Axis( vector_origin, ang, 16, 0.1, true )

	RENDERING_VR = true

	render.PushRenderTarget( rt )
		render.RenderView{
			origin = pos,
			angles = ang,
			drawviewmodel = true,
			fov = fov,
			viewmodelfov = fov,
			aspectratio = aspect,
			x = 0,
			y = 0,
			w = w / 2,
			h = h
		}
		render.RenderView{
			origin = pos,
			angles = ang,
			drawviewmodel = true,
			fov = fov,
			viewmodelfov = fov,
			aspectratio = aspect,
			x = w / 2,
			y = 0,
			w = w / 2,
			h = h
		}
	render.PopRenderTarget( rt )

	vr.Submit()

	RENDERING_VR = false
end )