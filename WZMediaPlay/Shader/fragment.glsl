#version 150 core
out vec4 FragColor;

in vec2 TexCoord;

uniform int iRenderInputSource; //0-- default, using VideoFile   1-- Use Camera
uniform int iStereoFlag;
uniform int iStereoInputFormat;
uniform int iStereoOutputFormat;
uniform bool bEnableRegion;
uniform vec4 VecRegion;
//�Ӳ����
uniform int iParallaxOffset;

// texture sampler
uniform sampler2D textureY;
uniform sampler2D textureU;
uniform sampler2D textureV;
uniform sampler2D textureUV;  // NV12 格式的 UV 交织纹理

// 视频格式：0=RGB, 1=NV12, 2=YUV420P
uniform int iVideoFormat;

vec3 normal_2d(sampler2D inputTexture, vec2 TexCoord) {
	vec3 rgb = texture(inputTexture, TexCoord).xyz;
	return rgb;
}

//InputType: LR, OutputType: Left-2D
vec3 stereo_lr_only_left(sampler2D inputTexture, vec2 TexCoord) {
	vec3 rgb = texture(inputTexture,vec2(TexCoord.x*0.5, TexCoord.y)).xyz;
	return rgb; 
}

vec3 stereo_rl_only_left(sampler2D inputTexture, vec2 TexCoord) {
	vec3 rgb = texture(inputTexture,vec2(TexCoord.x*0.5+0.5, TexCoord.y)).xyz;
	return rgb; 
}

vec3 stereo_ud_only_left(sampler2D inputTexture, vec2 TexCoord) {
	vec3 rgb = texture(inputTexture,vec2(TexCoord.x, TexCoord.y*0.5)).xyz;
	return rgb; 
}

//InputType: LR: OutputType: VerticalBarrier
vec3 stereo_lr_vertical_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture,0).xy;
	int pixelSize = 2;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}

	//  DstX/1.0 = (SrcX-upLeft.x)/(brX-topL.x)
	// => SrcX = (DstX*(brx-tLx))+upLeft.X
	// DstY/1.0 = (SrcY-upLeft.y)/(brY-tLY)
	// => SrcY = (DstY*(brY-tLY))+upLeft.y
	bool leftOrRightView = int(gl_FragCoord.x)%pixelSize==0?true:false;
	if(leftOrRightView) {
		//For Left, SRC*2=Target, Src=Target*0.5
		// for original(left view), target = (src*2) => src = target*0.5
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5,TexCoord.y)).xyz; //���ʼ�汾

		// then for parallax_shift
		// SRC*2+(parallax/2)/texWidth = TARGET
		// then, SRC = (target-parallax/2/texWidth)*0.5
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;  // �����Ӳ����
		float SrcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5; // ����Regionȫ�������Ӳ����
		float SrcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(SrcX<0) {
			rgb = vec3(0.0,1.0,1.0);
		} else if(SrcX>0.5) {
			rgb = vec3(1.0, 0.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(SrcX,SrcY)).xyz;
		}
	} else {
		//For Right, Target(asTexCoord)=0/0.5/1.0, SRC=Target*0.5+0.5=0.5/0.75/1.0
		// TARGET = (SRC-0.5)*2
		// for original, Target = (SRC-0.5)*2 => SRC = Target*0.5+0.5
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5+0.5, TexCoord.y)).xyz;

		// for ParallaxShift, Target = (SRC-0.5)*2-(parllax/2)/texWidth
		// then, SRC = (target+parallax/2/texWidth)*0.5 + 0.5
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float SrcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float SrcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(SrcX<0.5) {
			rgb = vec3(1.0,1.0,0.0);
		} else if(SrcX>1.0) {
			rgb = vec3(0.0, 0.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(SrcX,SrcY)).xyz;
		}
	}

	return rgb;
}

//InputType: LR, OutputType: HorizontalBarrier
vec3 stereo_lr_horizontal_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture,0).xy;
	int pixelSize = 2;
	
	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}

	// region & full combined version
	//  DstX/1.0 = (SrcX-upLeft.x)/(brX-topL.x)
	// => SrcX = (DstX*(brx-tLx))+upLeft.X
	// DstY/1.0 = (SrcY-upLeft.y)/(brY-tLY)
	// => SrcY = (DstY*(brY-tLY))+upLeft.y
	
	//Pre Version
	// TexCoord.x/1.0 = sx /(br.x-ul.x) => sx=TexCoord.x*(br.x-ul.x)
	// TexCoord.y / 1.0 = sy/(br.y-ul.y) => sy = TexCoord.y*(br.y-ul.y)
	bool leftOrRightView = int(gl_FragCoord.y)%pixelSize==0? true:false;
	if(leftOrRightView) {
	  	//For Left, SRC*2=Target, Src=Target*0.5
	  	// for original(left view), target = (src*2) => src = target*0.5
	  	// then for parallax_shift
	  	// SRC*2+(parallax/2)/texWidth = TARGET 
	  	// then, SRC = (target-parallax/2/texWidth)*0.5
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
			
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else {
		// for original, Target = (SRC-0.5)*2 => SRC = Target*0.5+0.5
		// for ParallaxShift, Target = (SRC-0.5)*2-(parllax/2)/texWidth
		// then, SRC = (target+parallax/2/texWidth)*0.5 + 0.5
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;

		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	}
	
	return rgb; 
}

vec3 stereo_lr_chess_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	int pixelSize = 2;
	vec2 texSize = textureSize(inputTexture,0).xy;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}

	
	// region & full combined version
	//  DstX/1.0 = (SrcX-upLeft.x)/(brX-topL.x)
	// => SrcX = (DstX*(brx-tLx))+upLeft.X
	// DstY/1.0 = (SrcY-upLeft.y)/(brY-tLY)
	// => SrcY = (DstY*(brY-tLY))+upLeft.y

	bool oddOrEvenRow = int(gl_FragCoord.y)%pixelSize==0?true:false; 
	bool oddOrEvenCol = int(gl_FragCoord.x)%pixelSize==0?true:false;
		
	if(oddOrEvenRow && oddOrEvenCol) {
		//OddRow,oddCol, use Left View 
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5,TexCoord.y)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;

		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else if(oddOrEvenRow && !oddOrEvenCol) {
		//OddRow, evenCol => RightView
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5+0.5, TexCoord.y)).xyz;
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else if(!oddOrEvenRow && oddOrEvenCol) {
		//EvenRow, oddCol => Use RightView
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5+0.5, TexCoord.y)).xyz;
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else if(!oddOrEvenRow && !oddOrEvenCol) {
		//EvenRow, evenCol => Use LeftView
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5, TexCoord.y)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else {
		//Should not enter here
		rgb = vec3(0.0,0.0,1.0);
	}

	return rgb; 
}


//Input-Type: RL , Output-Type: VerticalBarrier
vec3 stereo_rl_vertical_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture, 0).xy;
	int pixelSize = 2;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}

	bool leftOrRightView = int(gl_FragCoord.x)%pixelSize==0?true:false;
	if(leftOrRightView) {
		//LeftView, use right side of original image 
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;

		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else {
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	}
	
	return rgb; 
}


//Input-Type: RL, Output-Type; HorizontalBarrier
vec3 stereo_rl_horizontal_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture, 0).xy;
	int pixelSize = 2;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}
	
	bool leftOrRightView = int(gl_FragCoord.y)%pixelSize==0?true:false;
	if(leftOrRightView) {
	  	//LeftView, use right side of original image
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;

		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else {
		//Left Image for second line
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5,TexCoord.y)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	}
	
	return rgb; 
}


//Input:RL  Output: ChessBarrier
vec3 stereo_rl_chess_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	int pixelSize = 2;
	vec2 texSize = textureSize(inputTexture,0).xy;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}
	
	bool oddOrEvenRow = int(gl_FragCoord.y)%pixelSize==0? true: false;
	bool oddOrEvenCol = int(gl_FragCoord.x)%pixelSize==0? true: false;
	
	if(oddOrEvenRow && oddOrEvenCol) {
		//OddRow,oddCol, use Left View (original image is right side because RL format)
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5+0.5,TexCoord.y)).xyz;
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else if(oddOrEvenRow && !oddOrEvenCol) {
		//OddRow, evenCol => RightView (original image is left side because RL format)
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5, TexCoord.y)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else if(!oddOrEvenRow && oddOrEvenCol) {
		//EvenRow, oddCol => Use RightView (original image is left side because RL format)
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5, TexCoord.y)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else if(!oddOrEvenRow && !oddOrEvenCol) {
		//EvenRow, evenCol => Use LeftView (original image is right side because RL format)
		//rgb = texture(inputTexture, vec2(TexCoord.x*0.5+0.5, TexCoord.y)).xyz;
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY)).xyz;
		}
	} else {
		//Should not enter here
		rgb = vec3(0.0,0.0,1.0);
	}

	return rgb; 
}

//Input-Type: UD, Output-Type: VerticalBarrier
vec3 stereo_ud_vertical_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture,0).xy;
	int pixelSize = 2;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}
	
	// region & full combined version
	//  DstX/1.0 = (SrcX-upLeft.x)/(brX-topL.x)
	// => SrcX = (DstX*(brx-tLx))+upLeft.X
	// DstY/1.0 = (SrcY-upLeft.y)/(brY-tLY)
	// => SrcY = (DstY*(brY-tLY))+upLeft.y

	bool leftOrRightView = int(gl_FragCoord.x)%pixelSize==0? true:false;

	if(leftOrRightView) {
		//LeftView, for up-down original image, use the upper side
		//rgb = texture(inputTexture, vec2(TexCoord.x, TexCoord.y*0.5+0.5)).xyz;
		//float sx = TexCoord.x-float(iParallaxOffset/2)/texSize.x;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x);
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5)).xyz;
		}
	} else {
		//rgb = texture(inputTexture, vec2(TexCoord.x,TexCoord.y*0.5)).xyz;
		//float sx = TexCoord.x+float(iParallaxOffset/2)/texSize.x;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x);
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5+0.5)).xyz;
		}
	}
	
	return rgb; 
}

//Input-Type: UD, Output-Type: HorizontalBarrier
vec3 stereo_ud_horizontal_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture,0).xy;
	int pixelSize = 2;
	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}

	bool leftOrRightView = int(gl_FragCoord.y)%pixelSize==0?true:false;
	if(leftOrRightView) {
		//LeftView, for up-down original image, use the upper side
		//rgb = texture(inputTexture, vec2(TexCoord.x, TexCoord.y*0.5+0.5)).xyz; (ԭ����ϵ�����½ǣ����޸ĳ����Ͻ�)
		//float sx = TexCoord.x-float(iParallaxOffset/2)/texSize.x;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x);
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5)).xyz;
		}
	} else {
		//rgb = texture(inputTexture, vec2(TexCoord.x,TexCoord.y*0.5)).xyz;
		//float sx = TexCoord.x+float(iParallaxOffset/2)/texSize.x;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x);
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5+0.5)).xyz;
		}
	}
	
	return rgb; 
}

//Input-Type: UD, Output-Type: ChessBarrier 
vec3 stereo_ud_chess_barrier(sampler2D inputTexture, vec2 TexCoord, bool enableRegion, vec4 region, int iParallaxOffset) {	
	vec3 rgb;
	vec2 texSize = textureSize(inputTexture,0).xy;
	int pixelSize = 2;

	vec2 upLeft = vec2(0.0,0.0);
	vec2 bottomRight = vec2(1.0,1.0);
	if(enableRegion) {
		upLeft = region.xy;
		bottomRight = region.zw;
	}

	bool oddOrEvenRow = int(gl_FragCoord.y)%pixelSize==0? true: false;
	bool oddOrEvenCol = int(gl_FragCoord.x)%pixelSize==0? true: false;
	
	if(oddOrEvenRow && oddOrEvenCol) {
		//OddRow,oddCol, use Left View (original image is up-down, use the upside as leftview)
		//rgb = texture(inputTexture, vec2(TexCoord.x,TexCoord.y*0.5)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5)).xyz;
		}
	} else if(oddOrEvenRow && !oddOrEvenCol) {
		//OddRow, evenCol => RightView (original image is up-down, use the downside as rightview)
		//rgb = texture(inputTexture, vec2(TexCoord.x, TexCoord.y*0.5+0.5)).xyz;
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5+0.5)).xyz;
		}
	} else if(!oddOrEvenRow && oddOrEvenCol) {
		//EvenRow, oddCol => Use RightView (original image is up-down, use downside as rightview)
		//rgb = texture(inputTexture, vec2(TexCoord.x, TexCoord.y*0.5)).xyz;
		//float sx = (TexCoord.x+float(iParallaxOffset/2)/texSize.x)*0.5+0.5;
		float srcX = ((TexCoord.x+float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5+0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0.5) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>1.0) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5+0.5)).xyz;
		}
	} else if(!oddOrEvenRow && !oddOrEvenCol) {
		//EvenRow, evenCol => Use LeftView (original image is up-down, use upside as leftview)
		//rgb = texture(inputTexture, vec2(TexCoord.x, TexCoord.y*0.5)).xyz;
		//float sx = (TexCoord.x-float(iParallaxOffset/2)/texSize.x)*0.5;
		float srcX = ((TexCoord.x-float(iParallaxOffset/2)/texSize.x)*(bottomRight.x-upLeft.x)+upLeft.x)*0.5;
		float srcY = TexCoord.y*(bottomRight.y-upLeft.y)+upLeft.y;
		if(srcX<0) {
			rgb = vec3(1.0,1.0,1.0);
		} else if(srcX>0.5) {
			rgb = vec3(1.0, 1.0, 1.0);
		} else {
			rgb = texture(inputTexture, vec2(srcX,srcY*0.5)).xyz;
		}
	} else {
		//Should not enter here
		rgb = vec3(1.0,1.0,1.0);
	}

	return rgb; 
}


//iStereoFlag, 2d/3d, 0--for normal 2d, 1 --- for stereo(3d)
//iStereoInputFormat = default, 0 --- lr
//           = 1 -- rl
//           = 2 -- ud
// iStereoOutputFormat = default, 0 -- vertical-barrier
//                 = 1, horizontal-barrier
//                 = 2, chess-barrier
//                 = 3,  only-2d, if 3d, only display 2d(only left view)
vec3 stereo_display(sampler2D inputTexture, vec2 TexCoord, int iStereoFlag, int iStereoInputFormat, int iStereoOutputFormat, bool bEnableRegion, vec4 region, int iParallaxOffset) {
	if(iStereoFlag==0) {
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
}


void main()
{
	vec4 region = vec4(0.0,0.0, 1.0,1.0);
	if(bEnableRegion) {
		//region = vec4(VecRegion.x/float(gl_FragCoord.x), VecRegion.y/float(gl_FragCoord.y), VecRegion.z/float(gl_FragCoord.x), VecRegion.w/float(gl_FragCoord.y));
		region = VecRegion;
	}

	if(iRenderInputSource==1) {
		//Camera RenderInputSource
		vec3 rgb;
		//render to 2D(debug)
		//rgb.x = texture2D(textureY, TexCoord).r;
		//rgb.y = texture2D(textureY, TexCoord).g;
		//rgb.z = texture2D(textureY, TexCoord).b;
		rgb.xyz = stereo_display(textureY, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).xyz;
		
		FragColor = vec4(rgb,1.0);
	} else {
		//Video RenderInputSource
		// YUV到RGB转换（BT.601标准，适用于SD视频）
		// 注意：GL_LUMINANCE格式会自动将0-255归一化到0-1
		// Y值范围：16-235 (limited range) -> 0.0625-0.918，需要减去0.0625
		// U/V值范围：16-240 (centered at 128) -> 0.0625-0.938，需要减去0.5
		vec3 yuv, rgb;

		// 获取Y值（所有格式都从 textureY 采样）
		// GL_LUMINANCE格式会自动将0-255归一化到0-1
		float y = stereo_display(textureY, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;

		// 根据 iVideoFormat 区分 NV12 和 YUV420P 格式
		// iVideoFormat: 0=RGB, 1=NV12, 2=YUV420P
		float u, v;
		if (iVideoFormat == 1) {
			// NV12 格式：UV 交织在一个纹理中，.r=U, .g=V
			vec2 uv = stereo_display(textureUV, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).rg;
			u = uv.r;
			v = uv.g;
		} else {
			// YUV420P 格式：U 和 V 是独立的纹理
			u = stereo_display(textureU, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
			v = stereo_display(textureV, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
		}

		// YUV到RGB转换（BT.601标准）
		// 注意：GL_LUMINANCE格式已经将0-255归一化到0-1
		// Limited range: Y在16-235，U/V在16-240（centered at 128）
		// Full range: Y/U/V都在0-255
		yuv.x = y - 0.0625;  // Y: limited range (16-235) -> 减去16/256
		yuv.y = u - 0.5;     // U 分量
		yuv.z = v - 0.5;     // V 分量

		// BT.601 YUV到RGB转换矩阵
		// R = Y + 1.402 * (V - 0.5)
		// G = Y - 0.344 * (U - 0.5) - 0.714 * (V - 0.5)
		// B = Y + 1.772 * (U - 0.5)
		// 展开后：
		// R = 1.164*Y + 0.0*U + 1.596*V
		// G = 1.164*Y - 0.391*U - 0.813*V
		// B = 1.164*Y + 2.018*U + 0.0*V
		vec3 yuv2r = vec3(1.164, 0.0, 1.596);
		vec3 yuv2g = vec3(1.164, -0.391, -0.813);
		vec3 yuv2b = vec3(1.164, 2.018, 0.0);

		rgb.x = dot(yuv, yuv2r);
		rgb.y = dot(yuv, yuv2g);
		rgb.z = dot(yuv, yuv2b);

		// 限制RGB值在0-1范围内
		rgb = clamp(rgb, 0.0, 1.0);

		FragColor = vec4(rgb, 1.0);
	}
}

