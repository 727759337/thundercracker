--[[
    Sifteo Thundercracker firmware unit tests

    M. Elizabeth Scott <beth@sifteo.com> 
    Copyright <c> 2011 Sifteo, Inc. All rights reserved.
]]--

require('scripts/luaunit')
require('scripts/vram')

TestGraphics = {}

    function TestGraphics:setUp()
        gx:setUp()
    end

    function TestGraphics:test_solid()
        local colors = { 0x0000, 0xffff, 0x8000, 0x000f, 0x1234 }
        
        -- Plain solid colors
        gx:setMode(VM_SOLID)
        for k, color in pairs(colors) do
            gx:setColors{color}
            gx:drawAndAssert(string.format("solid-%04X", color))
        end
        
        -- Windowed rendering
        gx:setColors{0xF000}
        for k, win in pairs{ 1, 33, 128, 255, 0 } do
            gx:setWindow(71, win)
            gx:drawAndAssert(string.format("solid-win-%d", win, color))
        end
    end
    
    function TestGraphics:test_fb32()
        gx:setMode(VM_FB32)

        -- Horizontal gradient with a black outline, in a unique palette
        gx:xbFill(0, 512, 0)
        gx:setUniquePalette()
        for y = 1, 30, 1 do
            for x = 1, 30, 1 do
                gx:putPixelFB32(x, y, bit.band(x, 0xF))
            end
        end
        gx:drawAndAssert("fb32-outlined")
        
        -- Arbitrary pattern
        for i = 0, 511, 1 do
            gx.cube:xbPoke(i, i * 17)
        end
        gx:drawAndAssert("fb32-arbitrary")

        -- Diagonal gradient
        gx:xbFill(0, 512, 0)
        gx:setGrayscalePalette()
        for y = 0, 31, 1 do
            for x = 0, 31, 1 do
                gx:putPixelFB32(x, y, (x+y)*15/(31+31))
            end
        end
        gx:drawAndAssert("fb32-gradient")
        
        -- Windowed rendering
        gx:setColors{ 0x0F00 }
        gx:drawAndAssertWithWindow(VM_FB32, "fb32", {1, 4, 16, 17, 92, 255, 0})
    end

    function TestGraphics:test_fb64()
        gx:setMode(VM_FB64)

        -- Checkerboard, with a dark outline
        gx:xbFill(0, 512, 0)
        gx:setColors{gx:RGB565(0.5,0.5,0.5), gx:RGB565(1,1,0)}
        for y = 1, 62, 1 do
            for x = 1, 62, 1 do
                gx:putPixelFB64(x, y, bit.band(x+y, 1))
            end
        end
        gx:drawAndAssert("fb64-outlined")
        
        -- Binary counting pattern, reflected down the middle
        for i = 0, 255, 1 do
            gx.cube:xbPoke(i, i)
        end
        for i = 256, 511, 1 do
            gx.cube:xbPoke(i, 511-i)
        end
        gx:drawAndAssert("fb64-binary")

        -- Windowed rendering
        gx:drawAndAssertWithWindow(VM_FB64, "fb64", {1, 4, 16, 17, 92, 255, 0})       
    end
    
    function TestGraphics:test_fb128()
        gx:setMode(VM_FB128)

        -- Checkerboard, with a dark outline
        gx:xbFill(0, 0x300, 0)
        gx:setColors{gx:RGB565(0.5,0.5,0.5), gx:RGB565(0,1,0)}
        for x = 1, 126, 1 do
            for y = 1, 46, 1 do
                gx:putPixelFB128(x, y, bit.band(x+y, 1))
            end
        end
        gx:drawAndAssert("fb128-outlined")

        -- Binary counting pattern
        for i = 0, 0x2ff, 1 do
            gx.cube:xbPoke(i, bit.bxor(i, bit.rshift(i, 8)))
        end
        gx:drawAndAssert("fb128-binary")

        -- Windowed rendering
        gx:drawAndAssertWithWindow(VM_FB128, "fb128", {1, 2, 20, 48, 48*2, 255, 0})
    end
    
    function TestGraphics:test_rom()
        gx:setMode(VM_BG0_ROM)
        gx:drawROMPattern()
        gx:drawAndAssertWithBG0Pan("rom")
        gx:drawAndAssertWithWindow(VM_BG0_ROM, "rom", {1, 4, 16, 17, 92, 255, 0})       
    end
    
    function TestGraphics:test_rotation()
        -- Uses ROM mode, since that gives us an interesting high-res pattern to test with
        
        gx:setColors{0x1234}
        gx:panBG0(0,0)
        gx:drawROMPattern()
        
        for flags = 0, 7, 1 do        
            gx:setMode(VM_SOLID)
            gx:setWindow(0, 128)
            gx:drawFrame()
            gx:setMode(VM_BG0_ROM)
            gx:setWindow(15, 48)
            
            gx:xbReplace(VA_FLAGS, 5, 3, flags)
            gx:drawAndAssert(string.format("rotate-%d", flags))
        end
    end
    
    function TestGraphics:test_bg0()
        gx:setMode(VM_BG0)
        gx:drawBG0Pattern()
        gx:drawAndAssertWithBG0Pan("bg0")
        gx:drawAndAssertWithWindow(VM_BG0, "bg0", {1, 4, 16, 17, 92, 255, 0})       
    end
    
    function TestGraphics:test_bg2()
        -- Borrow the BG0 test pattern. It'll be reinterpreted as a 16x16 grid
        gx:setMode(VM_BG2)
        gx:drawBG0Pattern()
        
        -- Identity matrix for now
        gx:pokeWords(VA_BG2_BORDER, {0x1234})
        gx:setMatrixBG2(matrix:new(3, 'I'))
        gx:drawAndAssert("bg2-identity")
        
        -- With this matrix, do a windowing test.
        -- Note that the wrap tests (255, 0) will end up wrapping to the
        -- border color, not back to the tile array.
        gx:drawAndAssertWithWindow(VM_BG2, "bg2", {1, 4, 16, 17, 92, 255, 0})       
        gx:setWindow(0, 128)
       
        -- Integer scaling, without rotation or shearing
        local iScales = {0, 0.25, 0.5, 1, 2, 4, 8};
        for i, sx in pairs(iScales) do
            for i, sy in pairs(iScales) do
                gx:setMatrixBG2(gx:scaling(sx, sy))
                gx:drawAndAssert(string.format("bg2-scale-%04x-%04x", sx * 256, sy * 256))
            end
        end
        
        -- Panning
        for i = 1, 16 do
            gx:setMatrixBG2(gx:scaling(0.25, 0.25) * gx:translation(i, 2*i))
            gx:drawAndAssert(string.format("bg2-pan-%d", i))
        end

        -- Rotation about the center of the screen
        for i = 0, 360, 5 do
            gx:setMatrixBG2(gx:translation(64,64) * gx:rotation(i) * gx:translation(-64,-64))
            gx:drawAndAssert(string.format("bg2-rotate-%d", i))
        end
    end

    
-- You can use environment vars to pass in other options.
-- By default, we run all tests with no GUI.

gx:init(os.getenv("USE_FRONTEND"))
LuaUnit:run(os.getenv("TEST"))
gx:exit()
