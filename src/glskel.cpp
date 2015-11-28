#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cstdio>

namespace {

static constexpr auto const gWidth = 1024;
static constexpr auto const gHeight = 1024;

static GLchar const * gVsSrcRender = {
R"(#version 450 core

out vec2 v_uv;

layout(std140, binding = 0) uniform ViewportUniforms
{
	mat4 u_viewMatrix;
	vec4 u_viewport;
	float u_time;
};

const vec4 va[3] = vec4[]
	(
		vec4(-0.5, -0.5, 0.0, 0.0),
		vec4( 0.5,  0.5, 1.0, 1.0),
		vec4(-0.5,  0.5, 0.0, 1.0)
	);

void main()
{
	gl_Position = vec4(va[gl_VertexID].xy, 0.0, 1.0);
	v_uv = va[gl_VertexID].zw;
}
)"
};

static GLchar const * gFsSrcRender = {
R"(#version 450 core

in vec2 v_uv;
layout (location = 0) out vec4 fCol;

layout(std140, binding = 0) uniform ViewportUniforms
{
	mat4 u_viewMatrix;
	vec4 u_viewport;
	float u_time;
};

uniform sampler2D diffuse;

void main()
{
	fCol = texture2D(diffuse, v_uv);
}
)"
};

static GLchar const * gVsSrcPostProcess = {
R"(#version 450 core

layout(std140, binding = 0) uniform ViewportUniforms
{
	mat4 u_viewMatrix;
	vec4 u_viewport;
	float u_time;
};

const vec2 v[3] = vec2[]
	(
		vec2(-1.0, -3.0),
		vec2( 3.0,  1.0),
		vec2(-1.0,  1.0)
	);

void main()
{
	gl_Position=vec4(v[gl_VertexID], 0.0, 1.0);
}
)"
};

static GLchar const * gFsSrcPostProcess = {
R"(#version 450 core
layout (location = 0) out vec4 fCol;

layout(std140, binding = 0) uniform ViewportUniforms
{
	mat4 u_viewMatrix;
	vec4 u_viewport;
	float u_time;
};

uniform sampler2D previouspass;

void main()
{
	vec2 fc = gl_FragCoord.xy;
	float oddx = float(int(fc.x) % 2);
	float oddy = float(int(fc.y) % 2);
	fc.x += oddy*sin(u_time*2.0)*50.0;
	fc.y += oddx*cos(u_time*1.5)*50.0;
	vec2 uv = fc/vec2(u_viewport.z, u_viewport.w);
	fCol = vec4(texture2D(previouspass, uv).rgb, 1.0);
}
)"
};

void
keyCallback(
	GLFWwindow * const window,
	int const key,
	int const scancode,
	int const action,
	int const mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GL_TRUE);
	}
}

GLuint
createCheckerBoardTexture()
{
	constexpr auto width = 2;
	constexpr auto height = 2;
	unsigned char image[width*height*4] = {
		0x00, 0x00, 0x00, 0xff,
		0xa0, 0xa0, 0x00, 0xff,
		0x00, 0xa0, 0xa0, 0xff,
		0xff, 0xff, 0xff, 0xff
	};

	GLuint tex;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	if (tex) {
		glTextureStorage2D(tex, 1, GL_RGBA8, width, height);
		glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image);
		return tex;
	}
	std::fprintf(stderr, "Failed to create texture object\n");
	return tex;
}

GLuint
compileShader(GLenum type, char const* src)
{
	auto const shader = glCreateShader(type);
	if (shader) {
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);
		GLint compiled = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled) {
			return shader;
		}
		char infolog[4096];
		infolog[4095] = '\0';
		glGetShaderInfoLog(shader, sizeof infolog - 1, nullptr, infolog);
		glDeleteShader(shader);
		std::fprintf(stderr, "Compile error:\n0: ");
		for (auto i = 0u, j = 0u; src[i]; ++i) {
			auto const c = src[i];
			if (c == '\r') continue;
			if (c == '\n') {
				std::fprintf(stderr, "\n%u: ", ++j);
			} else {
				std::fputc(c, stderr);
			}
		}
		std::fprintf(stderr, "\n%s\n", infolog);
		return 0;
	}
	std::fprintf(stderr, "Failed to create shader object\n");
	return 0;
}

GLuint
linkProgram(GLuint const* shader, unsigned numShaderStages)
{
	auto const program = glCreateProgram();
	if (program) {
		for (auto i = 0u; i < numShaderStages; ++i) {
			glAttachShader(program, shader[i]);
		}
		glLinkProgram(program);
		GLint linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
#ifdef NDEBUG
		for (auto i = 0u; i < numShaderStages; ++i) {
			glDetachShader(program, shader[i]);
		}
#endif
		for (auto i = 0u; i < numShaderStages; ++i) {
			glDeleteShader(shader[i]);
		}
		if (linked) {
			return program;
		}
		char infolog[4096];
		infolog[4095] = '\0';
		glGetProgramInfoLog(program, sizeof infolog - 1, nullptr, infolog);
		glDeleteProgram(program);
		std::fprintf(stderr, "Link error:\n%s\n", infolog);
		return 0;
	}
	std::fprintf(stderr, "Failed to create program object\n");
	return 0;
}

struct ViewportUniforms {
	float viewMatrix[4*4];
	float viewport[4];
	float time;
	ViewportUniforms() : viewMatrix{ 0.0f }, viewport{ 0.0f }, time(0.0f) {}
};

} // !namespace

int
main()
{
	if (!glfwInit()) {
		std::fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 0);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

//	auto const window = glfwCreateWindow(windowWidth, windowHeight, "gltest", glfwGetPrimaryMonitor(), nullptr);
	auto const window = glfwCreateWindow(gWidth, gHeight, "glskel", nullptr, nullptr);
	if (!window) {
		std::fprintf(stderr, "Failed to open GLFW window\n");
		glfwTerminate();
		return -2;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	if (!gladLoadGL()) {
		std::fprintf(stderr, "Failed to load GLAD\n");
		return -3;
	}

	std::printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	struct ProgramType { enum { RENDER, POSTPROCESS, _size }; };
	GLuint program[ProgramType::_size][ProgramType::_size]; // XXX: check layout...
	{
		GLuint shader[2];
		shader[0] = compileShader(GL_VERTEX_SHADER, gVsSrcRender);
		if (!shader[0]) { return -4; }
		shader[1] = compileShader(GL_FRAGMENT_SHADER, gFsSrcRender);
		if (!shader[1]) { return -4; }
		auto const prg = linkProgram(shader, sizeof shader/sizeof(GLuint));
		if (!prg) { return -5; }
		program[ProgramType::RENDER][0] = prg;
		auto const idx = glGetUniformBlockIndex(prg, "ViewportUniforms");
		if (idx == GL_INVALID_INDEX) {
			std::fprintf(stderr, "Failed to find ViewportUniforms in render shader program\n");
			return -6;
		}
		glUniformBlockBinding(prg, idx, 0);
		program[ProgramType::RENDER][1] = idx;
	}
	{
		GLuint shader[2];
		shader[0] = compileShader(GL_VERTEX_SHADER, gVsSrcPostProcess);
		if (!shader[0]) { return -4; }
		shader[1] = compileShader(GL_FRAGMENT_SHADER, gFsSrcPostProcess);
		if (!shader[1]) { return -4; }
		auto const prg = linkProgram(shader, sizeof shader/sizeof(GLuint));
		if (!prg) { return -5; }
		program[ProgramType::POSTPROCESS][0] = prg;
		auto const idx = glGetUniformBlockIndex(prg, "ViewportUniforms");
		if (idx == GL_INVALID_INDEX) {
			std::fprintf(stderr, "Failed to find ViewportUniforms in postprocess shader program\n");
			return -6;
		}
		glUniformBlockBinding(prg, idx, 0);
		program[ProgramType::POSTPROCESS][1] = idx;
	}

	ViewportUniforms viewportUniforms;
	viewportUniforms.viewport[2] = gWidth;
	viewportUniforms.viewport[3] = gHeight;

	GLuint viewportUBO;
	glCreateBuffers(1, &viewportUBO);
	glNamedBufferStorage(viewportUBO, sizeof viewportUniforms, &viewportUniforms, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

	GLuint fbotex;
	glCreateTextures(GL_TEXTURE_2D, 1, &fbotex);
	glBindTexture(GL_TEXTURE_2D, fbotex);
	glTextureStorage2D(fbotex, 1, GL_RGBA8, gWidth, gHeight);
	GLuint fbo;
	glCreateFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbotex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::fprintf(stderr, "Framebuffer is not complete\n");
		glfwTerminate();
		return -10;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	auto const diffuse = createCheckerBoardTexture();
	if (!diffuse) {
		return -11;
	}

	glActiveTexture(GL_TEXTURE0 + 0);

	glUseProgram(program[ProgramType::RENDER][0]);
	glUniform1i(glGetUniformLocation(program[ProgramType::RENDER][0], "diffuse"), 0);

	glUseProgram(program[ProgramType::POSTPROCESS][0]);
	glUniform1i(glGetUniformLocation(program[ProgramType::POSTPROCESS][0], "previouspass"), 0);

	GLuint vao;
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	double startTime = glfwGetTime();
	do {
		auto const now = glfwGetTime();
		auto const t = float(now - startTime);
		viewportUniforms.viewMatrix[0] = 1.0f; // etc...
		viewportUniforms.time = t;
		glNamedBufferSubData(viewportUBO, 0, sizeof viewportUniforms, &viewportUniforms);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindBufferBase(GL_UNIFORM_BUFFER, program[ProgramType::RENDER][1], viewportUBO);

		glUseProgram(program[ProgramType::RENDER][0]);
		glBindTexture(GL_TEXTURE_2D, diffuse);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindBufferBase(GL_UNIFORM_BUFFER, program[ProgramType::POSTPROCESS][1], viewportUBO);

		glUseProgram(program[ProgramType::POSTPROCESS][0]);
		glBindTexture(GL_TEXTURE_2D, fbotex);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		glfwPollEvents();
	} while (!glfwWindowShouldClose(window));

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);

	// XXX: cleanup...

	glfwDestroyWindow(window);
	glfwTerminate();
}
