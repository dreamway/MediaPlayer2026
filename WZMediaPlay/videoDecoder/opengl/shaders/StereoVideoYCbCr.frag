#ifdef TEXTURE_RECTANGLE
    #define sampler sampler2DRect
    #define texCoordY  (vTexCoord * uTextureSize)
    #define texCoordUV (vTexCoord * uTextureSize / 2.0)
    #define texCoordYWithOffset(offset) ((vTexCoord + offset) * uTextureSize)
    #define texture texture2DRect
    // textureSize 对于 sampler2DRect 是内置的
    #define textureSize2D(sampler, lod) textureSize(sampler)
#else
    #define sampler sampler2D
    #define texCoordY  (vTexCoord)
    #define texCoordUV (vTexCoord)
    #define texCoordYWithOffset(offset) (vTexCoord + offset)
    #define texture texture2D
    
    // OpenGL 3.3+ 原生支持
    #define textureSize2D(sampler, lod) textureSize(sampler, lod)

#endif

varying vec2 vTexCoord;
uniform mat3 uYUVtRGB;
uniform vec2 uRangeMultiplier;
uniform float uBL;
uniform vec4 uVideoEq;
uniform float uSharpness;
uniform vec2 uTextureSize;
uniform float uBitsMultiplier;
uniform int uTrc;
uniform mat3 uColorPrimariesMatrix;
uniform float uMaxLuminance;
uniform int uNegative;
uniform sampler uY;
#ifdef NV12
    uniform sampler uCbCr;
#else
    uniform sampler uCb, uCr;
#endif

// 3D Stereo 参数（新增）
uniform int iStereoFlag;          // 0=2D, 1=3D
uniform int iStereoInputFormat;   // 0=LR, 1=RL, 2=UD
uniform int iStereoOutputFormat;  // 0=Vertical, 1=Horizontal, 2=Chess, 3=OnlyLeft
uniform bool bEnableRegion;       // 是否启用局部 3D 区域
uniform vec4 VecRegion;           // 局部 3D 区域坐标 (topLeftX, topLeftY, bottomRightX, bottomRightY)
uniform int iParallaxOffset;      // 视差偏移

#ifdef GL3
float getLumaAtOffset(float x, float y)
{
    return texture(uY, texCoordYWithOffset(vec2(x, y)))[0] - uBL;
}
#endif

// ========== 3D Stereo Shader 函数（从 FFmpegView 的 fragment.glsl 整合）==========

vec3 normal_2d(sampler2D inputTexture, vec2 TexCoord) {
    vec3 rgb = texture(inputTexture, TexCoord).xyz;
    return rgb;
}

//InputType: LR, OutputType: Left-2D
vec3 stereo_lr_only_left(sampler2D inputTexture, vec2 TexCoord) {
    vec3 rgb = texture(inputTexture, vec2(TexCoord.x*0.5, TexCoord.y)).xyz;
    return rgb; 
}

vec3 stereo_rl_only_left(sampler2D inputTexture, vec2 TexCoord) {
    vec3 rgb = texture(inputTexture, vec2(TexCoord.x*0.5+0.5, TexCoord.y)).xyz;
    return rgb; 
}

vec3 stereo_ud_only_left(sampler2D inputTexture, vec2 TexCoord) {
    vec3 rgb = texture(inputTexture, vec2(TexCoord.x, TexCoord.y*0.5)).xyz;
    return rgb; 
}

//InputType: LR: OutputType: VerticalBarrier
vec3 stereo_lr_vertical_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }

    // 使用 mod 函数替代 % 操作符（兼容性更好，不需要 GL_EXT_gpu_shader4）
    bool leftOrRightView = mod(int(gl_FragCoord.x), pixelSize) == 0 ? true : false;
    if(leftOrRightView) {
        float SrcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float SrcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(SrcX < 0.0) {
            rgb = vec3(0.0, 1.0, 1.0);
        } else if(SrcX > 0.5) {
            rgb = vec3(1.0, 0.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(SrcX, SrcY)).xyz;
        }
    } else {
        float SrcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float SrcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(SrcX < 0.5) {
            rgb = vec3(1.0, 1.0, 0.0);
        } else if(SrcX > 1.0) {
            rgb = vec3(0.0, 0.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(SrcX, SrcY)).xyz;
        }
    }
    return rgb;
}

//InputType: LR, OutputType: HorizontalBarrier
vec3 stereo_lr_horizontal_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;
    
    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }
    
    // 使用 mod 函数替代 % 操作符
    bool leftOrRightView = mod(int(gl_FragCoord.y), pixelSize) == 0 ? true : false;
    if(leftOrRightView) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    }
    return rgb; 
}

vec3 stereo_lr_chess_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    int pixelSize = 2;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }

    // 使用 mod 函数替代 % 操作符
    bool oddOrEvenRow = mod(int(gl_FragCoord.y), pixelSize) == 0 ? true : false; 
    bool oddOrEvenCol = mod(int(gl_FragCoord.x), pixelSize) == 0 ? true : false;
        
    if(oddOrEvenRow && oddOrEvenCol) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else if(oddOrEvenRow && !oddOrEvenCol) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else if(!oddOrEvenRow && oddOrEvenCol) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    }
    return rgb; 
}

//Input-Type: RL , Output-Type: VerticalBarrier
vec3 stereo_rl_vertical_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }

    // 使用 mod 函数替代 % 操作符（兼容性更好，不需要 GL_EXT_gpu_shader4）
    bool leftOrRightView = mod(int(gl_FragCoord.x), pixelSize) == 0 ? true : false;
    if(leftOrRightView) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    }
    return rgb; 
}

//Input-Type: RL, Output-Type; HorizontalBarrier
vec3 stereo_rl_horizontal_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }
    
    // 使用 mod 函数替代 % 操作符
    bool leftOrRightView = mod(int(gl_FragCoord.y), pixelSize) == 0 ? true : false;
    if(leftOrRightView) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    }
    return rgb; 
}

//Input:RL  Output: ChessBarrier
vec3 stereo_rl_chess_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    int pixelSize = 2;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }
    
    // 使用 mod 函数替代 % 操作符
    bool oddOrEvenRow = mod(int(gl_FragCoord.y), pixelSize) == 0 ? true : false;
    bool oddOrEvenCol = mod(int(gl_FragCoord.x), pixelSize) == 0 ? true : false;
    
    if(oddOrEvenRow && oddOrEvenCol) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else if(oddOrEvenRow && !oddOrEvenCol) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else if(!oddOrEvenRow && oddOrEvenCol) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY)).xyz;
        }
    }
    return rgb; 
}

//Input-Type: UD, Output-Type: VerticalBarrier
vec3 stereo_ud_vertical_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }
    
    // 使用 mod 函数替代 % 操作符（兼容性更好，不需要 GL_EXT_gpu_shader4）
    bool leftOrRightView = mod(int(gl_FragCoord.x), pixelSize) == 0 ? true : false;

    if(leftOrRightView) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x);
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x);
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5 + 0.5)).xyz;
        }
    }
    return rgb; 
}

//Input-Type: UD, Output-Type: HorizontalBarrier
vec3 stereo_ud_horizontal_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;
    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }

    // 使用 mod 函数替代 % 操作符
    bool leftOrRightView = mod(int(gl_FragCoord.y), pixelSize) == 0 ? true : false;
    if(leftOrRightView) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x);
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x);
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5 + 0.5)).xyz;
        }
    }
    return rgb; 
}

//Input-Type: UD, Output-Type: ChessBarrier 
vec3 stereo_ud_chess_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
    vec3 rgb;
    vec2 texSize = textureSize2D(inputTexture, 0).xy;
    int pixelSize = 2;

    vec2 upLeft = vec2(0.0, 0.0);
    vec2 bottomRight = vec2(1.0, 1.0);
    if(enableRegion) {
        upLeft = region.xy;
        bottomRight = region.zw;
    }

    // 使用 mod 函数替代 % 操作符
    bool oddOrEvenRow = mod(int(gl_FragCoord.y), pixelSize) == 0 ? true : false;
    bool oddOrEvenCol = mod(int(gl_FragCoord.x), pixelSize) == 0 ? true : false;
    
    if(oddOrEvenRow && oddOrEvenCol) {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5)).xyz;
        }
    } else if(oddOrEvenRow && !oddOrEvenCol) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5 + 0.5)).xyz;
        }
    } else if(!oddOrEvenRow && oddOrEvenCol) {
        float srcX = ((TexCoord.x + float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5 + 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 1.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5 + 0.5)).xyz;
        }
    } else {
        float srcX = ((TexCoord.x - float(iParallaxOffset/2)/texSize.x) * (bottomRight.x - upLeft.x) + upLeft.x) * 0.5;
        float srcY = TexCoord.y * (bottomRight.y - upLeft.y) + upLeft.y;
        if(srcX < 0.0) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else if(srcX > 0.5) {
            rgb = vec3(1.0, 1.0, 1.0);
        } else {
            rgb = texture(inputTexture, vec2(srcX, srcY * 0.5)).xyz;
        }
    }
    return rgb; 
}

// 3D Stereo 主函数
vec3 stereo_display(sampler2D inputTexture, vec2 TexCoord, int iStereoFlag, int iStereoInputFormat, int iStereoOutputFormat, bool bEnableRegion, vec4 region, int iParallaxOffset) {
    if(iStereoFlag == 0) {
        return normal_2d(inputTexture, TexCoord);
    } else {
        switch(iStereoInputFormat) {
        case 0:  // LR
        default:
            switch(iStereoOutputFormat) {
            case 0:
            default:
                return stereo_lr_vertical_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 1:
                return stereo_lr_horizontal_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 2:
                return stereo_lr_chess_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 3:
                return stereo_lr_only_left(inputTexture, TexCoord);
            }
            break;
        case 1: // RL 
            switch(iStereoOutputFormat) {
            case 0:
            default:
                return stereo_rl_vertical_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 1:
                return stereo_rl_horizontal_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 2:
                return stereo_rl_chess_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 3:
                return stereo_rl_only_left(inputTexture, TexCoord);
            }
            break;
        case 2: //UD
            switch(iStereoOutputFormat) {
            case 0:
            default:
                return stereo_ud_vertical_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 1:
                return stereo_ud_horizontal_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 2:
                return stereo_ud_chess_barrier(inputTexture, TexCoord, bEnableRegion, region, iParallaxOffset);
            case 3:
                return stereo_ud_only_left(inputTexture, TexCoord);
            }
            break;
        }
    }
    return normal_2d(inputTexture, TexCoord);  // fallback
}

// ========== 2D 渲染逻辑（基于 VideoYCbCr.frag）==========

void main()
{
    float brightness = uVideoEq[0];
    vec3 contrastSaturation = vec3(
        uVideoEq[1],
        uVideoEq[1] * uVideoEq[2],
        uVideoEq[1] * uVideoEq[2]
    );

#ifdef NV12
    vec3 YCbCr = vec3(
        texture(uY   , texCoordY )[0],
        texture(uCbCr, texCoordUV).xy
    );
#else
    vec3 YCbCr = vec3(
        texture(uY , texCoordY )[0],
        texture(uCb, texCoordUV)[0],
        texture(uCr, texCoordUV)[0]
    );
#endif
    YCbCr *= uBitsMultiplier;
    YCbCr -= vec3(uBL, vec2(128.0 / 255.0));

#ifdef GL3
    if (uSharpness != 0.0)
    {
        vec2 single = 1.0 / uTextureSize;
        float lumaBlur = (
            getLumaAtOffset(-single.x, -single.y) / 16.0 + getLumaAtOffset(0.0, -single.y) / 8.0 + getLumaAtOffset(single.x, -single.y) / 16.0 +
            getLumaAtOffset(-single.x,  0.0     ) /  8.0 + YCbCr[0]                        / 4.0 + getLumaAtOffset(single.x,  0.0     ) /  8.0 +
            getLumaAtOffset(-single.x,  single.y) / 16.0 + getLumaAtOffset(0.0,  single.y) / 8.0 + getLumaAtOffset(single.x,  single.y) / 16.0
        );
        YCbCr[0] = clamp(YCbCr[0] + (YCbCr[0] - lumaBlur) * uSharpness, 0.0, 1.0);
    }

    float hueAdj = uVideoEq[3];
    if (hueAdj != 0.0)
    {
        float hue = atan(YCbCr[2], YCbCr[1]) + hueAdj;
        float chroma = sqrt(YCbCr[1] * YCbCr[1] + YCbCr[2] * YCbCr[2]);
        YCbCr[1] = chroma * cos(hue);
        YCbCr[2] = chroma * sin(hue);
    }
#endif

    vec3 rgb = clamp(uYUVtRGB * ((YCbCr * uRangeMultiplier.xyy - vec3(0.5, 0.0, 0.0)) * contrastSaturation + vec3(0.5, 0.0, 0.0)), 0.0, 1.0);

#ifdef GL3
    if (uTrc == AVCOL_TRC_BT709)
    {
        colorspace_trc_bt709(rgb, uColorPrimariesMatrix);
    }
    else if (uTrc == AVCOL_TRC_SMPTE2084)
    {
        colorspace_trc_smpte2084(rgb, uColorPrimariesMatrix, uMaxLuminance);
    }
    else if (uTrc == AVCOL_TRC_ARIB_STD_B67)
    {
        colorspace_trc_hlg(rgb, uColorPrimariesMatrix, uMaxLuminance);
    }
    if (uNegative != 0)
    {
        rgb = 1.0 - rgb;
    }
#endif

    // ========== 3D Stereo 处理（新增）==========
    // 如果启用 3D，对 Y 通道应用 3D 处理
    // 注意：这里我们需要对 YCbCr 的 Y 通道应用 3D 处理，然后转换回 RGB
    // 但由于 shader 的限制，我们需要在 YCbCr 到 RGB 转换之前处理
    
    // 实际上，3D 处理应该在 YCbCr 空间进行，但为了简化，我们在 RGB 空间处理
    // 更准确的做法是：对 Y 通道应用 3D，然后转换
    
    // 临时方案：先转换到 RGB，然后对 RGB 应用 3D（这不是最优的，但可以工作）
    // 理想方案：对 Y 通道应用 3D，然后转换到 RGB
    
    // 由于 3D 处理需要访问原始纹理，我们需要重新采样
    // 这里我们使用一个简化的方法：如果启用 3D，对最终的 RGB 应用 3D 处理
    
    // 注意：由于我们已经转换到 RGB，3D 处理需要访问原始 Y 纹理
    // 为了正确实现，我们需要在 YCbCr 阶段应用 3D，但这需要重构 shader
    
    // 暂时：如果启用 3D，我们使用 stereo_display 函数处理 Y 通道
    // 但这需要重新采样，性能可能受影响
    
    // 简化实现：如果 iStereoFlag == 0，使用正常的 2D 渲染
    // 如果 iStereoFlag != 0，我们需要重新采样 Y 通道并应用 3D
    
    // 由于 shader 的限制，我们暂时在 RGB 阶段应用 3D（这不是最优的）
    // 后续可以优化为在 YCbCr 阶段应用 3D
    
    vec4 region = vec4(0.0, 0.0, 1.0, 1.0);
    if(bEnableRegion) {
        region = VecRegion;
    }
    
    // 如果启用 3D，对 Y 通道应用 3D 处理
    if(iStereoFlag != 0) {
        // 重新采样 Y 通道并应用 3D
        float yValue = stereo_display(uY, vTexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
        
        // 重新计算 YCbCr（使用 3D 处理后的 Y）
        YCbCr = vec3(
            yValue,
            texture(uCb, texCoordUV)[0],
            texture(uCr, texCoordUV)[0]
        );
        YCbCr *= uBitsMultiplier;
        YCbCr -= vec3(uBL, vec2(128.0 / 255.0));
        
        // 重新计算 RGB
        rgb = clamp(uYUVtRGB * ((YCbCr * uRangeMultiplier.xyy - vec3(0.5, 0.0, 0.0)) * contrastSaturation + vec3(0.5, 0.0, 0.0)), 0.0, 1.0);
        
#ifdef GL3
        if (uTrc == AVCOL_TRC_BT709)
        {
            colorspace_trc_bt709(rgb, uColorPrimariesMatrix);
        }
        else if (uTrc == AVCOL_TRC_SMPTE2084)
        {
            colorspace_trc_smpte2084(rgb, uColorPrimariesMatrix, uMaxLuminance);
        }
        else if (uTrc == AVCOL_TRC_ARIB_STD_B67)
        {
            colorspace_trc_hlg(rgb, uColorPrimariesMatrix, uMaxLuminance);
        }
        if (uNegative != 0)
        {
            rgb = 1.0 - rgb;
        }
#endif
    }

    gl_FragColor = vec4(rgb + brightness, 1.0);
}
