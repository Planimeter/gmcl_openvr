require( "openvr" )

local w, h
local aspect
local fov
local rt

local function debugprint( str, x, y )
	draw.SimpleTextOutlined( str, "DebugFixed", x, y, color_white, TEXT_ALIGN_LEFT, TEXt_ALIGN_BOTTOM, 1, color_black )
end

RunConsoleCommand( "gmod_mcore_test", 0 )

hook.Add( "RenderScene", "RenderVR", function()
	if not vr.Ready() then return end

	if not rt then
		w, h, aspect, fov = vr.GetViewParameters()

		vr.CaptureTexture()
		rt = GetRenderTarget( "vr", w, h, false )
	end

	if not rt then return end

	local pose = vr.GetDevicePose( 0 )
	--pose.pose[ 4 ] = { 0, 0, 0, 0 }

	local pos = Vector()
	pos.x = pose.pose[ 1 ][ 4 ] * 52.52100840336134
	pos.y = -pose.pose[ 3 ][ 4 ] * 52.52100840336134
	pos.z = pose.pose[ 2 ][ 4 ] * 52.52100840336134

	local ang = Angle()

	local vr_forward = Vector( -pose.pose[ 1 ][ 3 ], -pose.pose[ 2 ][ 3 ], -pose.pose[ 3 ][ 3 ] )
	local vr_right = Vector( pose.pose[ 1 ][ 1 ], pose.pose[ 2 ][ 1 ], pose.pose[ 3 ][ 1 ] )
	local vr_up = Vector( pose.pose[ 1 ][ 2 ], pose.pose[ 2 ][ 2 ], pose.pose[ 3 ][ 2 ] )

	local forward = Vector( vr_forward.x, -vr_forward.z, vr_forward.y )
	local right = Vector( vr_right.x, -vr_right.z, vr_right.y )
	local up = Vector( vr_up.x, -vr_up.z, vr_up.y )

    local sr, sp, sy, cr, cp, cy;

	sp = -forward.z

	local cp_x_cy = forward.x
	local cp_x_sy = forward.y
	local cp_x_sr = -right.z
	local cp_x_cr = up.z

	local yaw = math.atan2( cp_x_sy, cp_x_cy )
	local roll = math.atan2( cp_x_sr, cp_x_cr )

	cy = math.cos( yaw )
	sy = math.sin( yaw )
	cr = math.cos( roll )
	sr = math.sin( roll )

	local EPSILON = 0.0001

	if (math.abs(cy) > EPSILON) then
		cp = cp_x_cy / cy
	elseif (math.abs(sy) > EPSILON) then
		cp = cp_x_sy / sy
	elseif (math.abs(sr) > EPSILON) then
		cp = cp_x_sr / sr
	elseif (math.abs(cr) > EPSILON) then
		cp = cp_x_cr / cr
	else
		cp = math.cos( math.asin( sp ) )
	end

	local pitch = math.atan2( sp, cp );

	ang.p = -math.NormalizeAngle( 180 - ( 360 - ( pitch / ( math.pi * 2 / 360 ) ) ) )
	ang.y = -math.NormalizeAngle( 180 - ( yaw / ( math.pi * 2 / 360 ) ) )
	ang.r = -math.NormalizeAngle( 180 - ( roll / ( math.pi * 2 / 360 ) ) )

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

	render.PushRenderTarget( rt )
		render.RenderView{
			origin = pos,
			angles = ang,
			fov = fov,
			aspectratio = aspect,
			x = 0,
			y = 0,
			w = w / 2,
			h = h
		}
		render.RenderView{
			origin = pos,
			angles = ang,
			fov = fov,
			aspectratio = aspect,
			x = w / 2,
			y = 0,
			w = w / 2,
			h = h
		}
	render.PopRenderTarget( rt )
end )