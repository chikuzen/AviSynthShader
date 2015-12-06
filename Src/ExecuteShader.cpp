#include "ExecuteShader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

ExecuteShader::ExecuteShader(PClip _child, PClip _clip1, PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _precision, int _precisionIn, int _precisionOut, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), precisionIn(_precisionIn), precisionOut(_precisionOut) {

	m_clips[0] = _clip1;
	m_clips[1] = _clip2;
	m_clips[2] = _clip3;
	m_clips[3] = _clip4;
	m_clips[4] = _clip5;
	m_clips[5] = _clip6;
	m_clips[6] = _clip7;
	m_clips[7] = _clip8;
	m_clips[8] = _clip9;

	// Validate parameters
	if (!vi.IsY8())
		env->ThrowError("ExecuteShader: Source must be a command chain");
	if (precision < 1 || precision > 3)
		env->ThrowError("ExecuteShader: Precision must be 1, 2 or 3");

	// Initialize
	dummyHWND = CreateWindowA("STATIC", "dummy", 0, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

	// We must change pixel type here for the next filter to recognize it properly during its initialization
	srcHeight = vi.height;
	vi.pixel_type = VideoInfo::CS_BGR32;

	InitializeDevice(env);
}

ExecuteShader::~ExecuteShader() {
	delete render;
	DestroyWindow(dummyHWND);
}

void ExecuteShader::InitializeDevice(IScriptEnvironment* env) {
	render = new D3D9RenderImpl();
	if (FAILED(render->Initialize(dummyHWND, precision, precisionIn, precisionOut)))
		env->ThrowError("ExecuteShader: Initialize failed.");

	// We only need to know the difference between precision 2 and 3 to initialize video buffers. Then, both are 16 bits.
	if (precision == 3)
		precision = 2;
	if (precisionIn == 3)
		precisionIn = 2;
	if (precisionOut == 3)
		precisionOut = 2;

	CreateInputClip(0, env);
	CreateInputClip(1, env);
	CreateInputClip(2, env);
	CreateInputClip(3, env);
	CreateInputClip(4, env);
	CreateInputClip(5, env);
	CreateInputClip(6, env);
	CreateInputClip(7, env);
	CreateInputClip(8, env);

	PVideoFrame src = child->GetFrame(0, env);
	const byte* srcReader = src->GetReadPtr();

	// Each row of input clip contains commands to execute. Create one texture for each command.
	CommandStruct cmd;
	InputTexture* texture;
	int OutputWidth, OutputHeight;
	bool IsLast;
	render->ResetTextureClipIndex();
	for (int i = 0; i < srcHeight; i++) {
		memcpy(&cmd, srcReader, sizeof(CommandStruct));
		srcReader += src->GetPitch();

		ConfigureShader(&cmd, env);

		texture = render->FindTextureByClipIndex(cmd.OutputIndex, env);
		// If clip at output position isn't defined, use dimensions of first clip by default.
		if (texture->Texture == NULL)
			texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
		OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
		OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;
		IsLast = i == srcHeight - 1; // Only create memory on the CPU side for the last command, as it is the only one that needs to be read back.
		if (FAILED(render->CreateInputTexture(9 + i, cmd.OutputIndex, OutputWidth, OutputHeight, IsLast, IsLast)))
			env->ThrowError("ExecuteShader: Failed to create input texture.");

		if (IsLast) {
			if (cmd.OutputIndex != 1)
				env->ThrowError("ExecuteShader: Last command must have Output = 1");

			vi.width = OutputWidth * precisionOut;
			vi.height = OutputHeight;
		}
	}
	render->ResetTextureClipIndex();
}

PVideoFrame __stdcall ExecuteShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);
	const byte* srcReader = src->GetReadPtr();

	// Each row of input clip contains commands to execute
	CommandStruct cmd;
	bool IsLast;
	InputTexture* texture;
	int OutputWidth, OutputHeight;
	render->ResetTextureClipIndex();
	for (int i = 0; i < srcHeight; i++) {
		memcpy(&cmd, srcReader, sizeof(CommandStruct));
		srcReader += src->GetPitch();
		IsLast = i == srcHeight - 1;

		// Copy input clips from AviSynth
		for (int j = 0; j < 9; j++) {
			CopyInputClip(j, n, env);
		}

		texture = render->FindTextureByClipIndex(cmd.OutputIndex, env);
		// If clip at output position isn't defined, use dimensions of first clip by default.
		if (texture->Texture == NULL)
			texture = render->FindTextureByClipIndex(cmd.ClipIndex[0], env);
		OutputWidth = cmd.OutputWidth > 0 ? cmd.OutputWidth : texture->Width;
		OutputHeight = cmd.OutputHeight > 0 ? cmd.OutputHeight : texture->Height;

		// Configure pixel shader
		for (int i = 0; i < 9; i++) {
			if (cmd.Param[i].Type != ParamType::None) {
				if (FAILED(render->SetPixelShaderConstant(i, &cmd.Param[i])))
					env->ThrowError("ExecuteShader failed to set parameters.");
			}
		}

		if FAILED(render->ProcessFrame(&cmd, OutputWidth, OutputHeight, IsLast, env))
			env->ThrowError("ExecuteShader: ProcessFrame failed.");
	}

	// After last command, copy result back to AviSynth.
	PVideoFrame dst = env->NewVideoFrame(vi);
	render->CopyBufferToAviSynth(srcHeight - 1, dst->GetWritePtr(), dst->GetPitch(), env);
	return dst;
}

void ExecuteShader::CreateInputClip(int index, IScriptEnvironment* env) {
	// clip1-clip9 take texture spots 0-8. Then, each shader execution will output in subsequent texture spots.
	PClip clip = m_clips[index];
	if (clip != NULL) {
		if (!clip->GetVideoInfo().IsRGB32())
			env->ThrowError("ExecuteShader: You must first call ConvertToShader on source");

		if (FAILED(render->CreateInputTexture(index, index + 1, clip->GetVideoInfo().width / precisionIn, clip->GetVideoInfo().height, true, false)))
			env->ThrowError("ExecuteShader: Failed to create input textures.");
	}
	else
		render->m_InputTextures[index].ClipIndex = index + 1;
}

// Copies the first clip from AviSynth before running the first command.
void ExecuteShader::CopyInputClip(int index, int n, IScriptEnvironment* env) {
	PClip clip = m_clips[index];
	if (m_clips[index] != NULL) {
		PVideoFrame frame = clip->GetFrame(n, env);
		if (FAILED(render->CopyAviSynthToBuffer(frame->GetReadPtr(), frame->GetPitch(), index, clip->GetVideoInfo().width / precisionIn, clip->GetVideoInfo().height, env)))
			env->ThrowError("ExecuteShader: CopyInputClip failed");
	}
}

void ExecuteShader::ConfigureShader(CommandStruct* cmd, IScriptEnvironment* env) {
	if FAILED(render->InitPixelShader(cmd, env)) {
		char* ErrorText = "Shader: Failed to open pixel shader ";
		char* FullText;
		int TextLength = strlen(ErrorText) + strlen(cmd->Path) + 1;
		FullText = (char*)malloc(TextLength);
		strcpy_s(FullText, TextLength, ErrorText);
		strcat_s(FullText, TextLength, cmd->Path);
		env->ThrowError(FullText);
		free(FullText);
	}
}
