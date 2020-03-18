#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <thread>

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 uv;
};

std::vector<Vertex> load_mesh(const std::string& filename, std::string& diffuse_texture)
{
	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_PTV_NORMALIZE, 1);
	const aiScene* scene = importer.ReadFile(filename, aiProcess_PreTransformVertices | aiProcess_Triangulate | 
		aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "Assimp error: " << importer.GetErrorString() << std::endl;
		return {};
	}

	if (scene->mNumMeshes < 1)
	{
		std::cout << "No meshes found" << std::endl;
		return {};
	}

	auto mesh = scene->mMeshes[0];

	auto transform = scene->mRootNode->mTransformation;
	aiQuaternion rot;
	aiVector3D pos;
	transform.DecomposeNoScaling(rot, pos);
	auto rotation = rot.GetMatrix();

	std::vector<Vertex> result;
	result.resize(mesh->mNumVertices);

	for(unsigned i = 0; i < mesh->mNumVertices; ++i)
	{
		auto v = transform * mesh->mVertices[i];
		for (unsigned j = 0; j < 3; ++j)
			result[i].pos[j] = v[j];
		for (unsigned j = 0; j < 2; ++j)
			result[i].uv[j] = mesh->HasTextureCoords(0) ? glm::fract(mesh->mTextureCoords[0][i][j]) : 0.0f;
	}

	auto material = scene->mMaterials[mesh->mMaterialIndex];
	
	if (material->GetTextureCount(aiTextureType::aiTextureType_DIFFUSE) > 0)
	{
		aiString path;
		material->GetTexture(aiTextureType::aiTextureType_DIFFUSE, 0, &path);
		diffuse_texture = path.C_Str();
		std::cout << filename << " has diffuse texture " << diffuse_texture << std::endl;
	}

	return result;
}

const char* vert_code =
"#version 330 core\n"
"layout(location = 0) in vec3 v_pos;\n"
"layout(location = 1) in vec2 v_coord;\n"

"uniform mat4 MVP;\n"

"out vec2 f_coord;\n"

"void main()\n"
"{\n"
	"gl_Position = MVP * vec4(v_pos, 1.0);\n"
	"f_coord = v_coord;\n"
"}\n";

const char* frag_code =
"#version 330 core\n"
"out vec4 FragColor;\n"
"uniform sampler2D tex;\n"

"in vec2 f_coord;\n"

"void main()\n"
"{\n"
	"gl_FragData[0] = texture(tex, f_coord);\n"
	"gl_FragData[1] = vec4(f_coord, 0, 1);\n"
"}\n";

int main(int argc, char *argv[])
{
	try
	{
		cxxopts::Options options("s_gen", "Generates samples");
		options
			.add_options()
			("i,input", "input mesh", cxxopts::value<std::string>()->default_value("foo.fbx"))
			("o,output", "output folder (where to save samples)", cxxopts::value<std::string>()->default_value("out"))
			("n", "sample count", cxxopts::value<unsigned>()->default_value("128"))
			("w", "image width", cxxopts::value<unsigned>()->default_value("512"))
			("h", "image height", cxxopts::value<unsigned>()->default_value("512"))
			("min_fov", "min fov", cxxopts::value <float>()->default_value("30.0"))
			("max_fov", "max fov", cxxopts::value<float>()->default_value("75.0"))
			("min_r", "min radius", cxxopts::value<float>()->default_value("1.25"))
			("max_r", "max radius", cxxopts::value<float>()->default_value("2.0"))
			("help", "prints help");


		auto result = options.parse(argc, argv);

		if (result.count("help"))
		{
			std::cout << "Flags: " << std::endl;
			std::cout << "-i <mesh filename>" << std::endl;
			std::cout << "-o <output folder>" << std::endl;
			std::cout << "-n <sample count>" << std::endl;
			std::cout << "-w <sample width>" << std::endl;
			std::cout << "-h <sample height>" << std::endl;
			std::cout << "--min_fov <min fov>" << std::endl;
			std::cout << "--max_fov <max fov>" << std::endl;
			std::cout << "--min_r <min radius>" << std::endl;
			std::cout << "--max_r <max radius>" << std::endl;
			return EXIT_SUCCESS;
		}

		std::string mesh_filename = result["input"].as<std::string>();
		std::string out = result["output"].as<std::string>();
		float min_radius = result["min_r"].as<float>();
		float max_radius = result["max_r"].as<float>();

		if (min_radius > max_radius)
			std::swap(min_radius, max_radius);

		//std::cout << min_radius << " " << max_radius << std::endl;

		float min_fov = result["min_fov"].as<float>();
		float max_fov = result["max_fov"].as<float>();

		if (min_fov > max_fov)
			std::swap(min_fov, max_fov);

		//std::cout << min_fov << " " << max_fov << std::endl;

		unsigned width = result["w"].as<unsigned>();
		unsigned height = result["h"].as<unsigned>();

		unsigned n = result["n"].as<unsigned>();

		const float aspect = width / float(height);

		std::cout << "Input mesh: " << mesh_filename << std::endl;
		std::cout << "Output directory: " << out << std::endl;
		std::cout << "Sample count: " << n << std::endl;
		std::cout << "Sample size: " << width << "x" << height << std::endl;
		std::cout << "Aspect ratio: " << aspect << std::endl;
		std::cout << "FOV range: " << min_fov << "-" << max_fov << std::endl;
		std::cout << "Radius range: " << min_radius << "-" << max_radius << std::endl;

		std::cout << "Opening file " << mesh_filename << "..." << std::endl;

		std::string diffuse;
		auto mesh = load_mesh(mesh_filename, diffuse);

		if (diffuse.empty())
		{
			std::cout << "No diffuse texture" << std::endl;
			return EXIT_FAILURE;
		}

		std::cout << "Vertices count: " << mesh.size() << std::endl;

		if (glfwInit() == GLFW_FALSE)
		{
			std::cout << "GLFW initialization failed" << std::endl;
			return EXIT_FAILURE;
		}

		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		auto w = glfwCreateWindow(int(width), int(height), "", nullptr, nullptr);

		if (w == nullptr)
		{
			std::cout << "Context creation failed" << std::endl;
			return EXIT_FAILURE;
		}

		glfwMakeContextCurrent(w);
		glfwSwapInterval(0);

		glewExperimental = true;
		if (glewInit() != GLEW_OK)
		{
			std::cout << "GLEW initialization failed" << std::endl;
			glfwTerminate();
			return EXIT_FAILURE;
		}

		GLuint tex = 0;

		if (!diffuse.empty())
		{
			int w, h, c;
			auto tex_data = stbi_load(diffuse.c_str(), &w, &h, &c, 3);

			if (tex_data != nullptr)
			{
				glCreateTextures(GL_TEXTURE_2D, 1, &tex);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, tex);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, tex_data);
				glGenerateMipmap(GL_TEXTURE_2D);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			else
			{
				std::cout << "Can't load " << diffuse << std::endl;
				glfwTerminate();
				return EXIT_FAILURE;
			}

			STBI_FREE(tex_data);
		}

		GLuint vao = 0;

		glCreateVertexArrays(1, &vao);
		glBindVertexArray(vao);

		GLuint vbo = 0;

		glCreateBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(Vertex), mesh.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
		glEnableVertexAttribArray(1);
		glBindVertexArray(0);

		GLuint prog = 0;
		{
			GLuint vert = glCreateShader(GL_VERTEX_SHADER);
			GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

			glShaderSource(vert, 1, &vert_code, 0);
			glCompileShader(vert);

			char infoLog[512];
			int success;

			glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
			if (!success)
			{
				glGetShaderInfoLog(vert, 512, nullptr, infoLog);
				std::cout << "Vertex shader error: " << infoLog << std::endl;
				glfwTerminate();
				return EXIT_FAILURE;
			};

			glShaderSource(frag, 1, &frag_code, 0);
			glCompileShader(frag);

			glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
			if (!success)
			{
				glGetShaderInfoLog(frag, 512, nullptr, infoLog);
				std::cout << "Fragment shader error: " << infoLog << std::endl;
				glfwTerminate();
				return EXIT_FAILURE;
			};

			prog = glCreateProgram();
			glAttachShader(prog, vert);
			glAttachShader(prog, frag);
			glLinkProgram(prog);

			glGetProgramiv(prog, GL_LINK_STATUS, &success);
			if (!success)
			{
				glGetProgramInfoLog(prog, 512, NULL, infoLog);
				std::cout << "Program linking error: " << infoLog << std::endl;
				glfwTerminate();
				return EXIT_FAILURE;
			}

			glDeleteShader(vert);
			glDeleteShader(frag);
		}

		int fb_w, fb_h;
		glfwGetFramebufferSize(w, &fb_w, &fb_h);
		glViewport(0, 0, fb_w, fb_h);

		GLuint depth_tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &depth_tex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depth_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
			fb_w, fb_h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GLuint color_tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &color_tex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, color_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			fb_w, fb_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLuint uv_tex = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &uv_tex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, uv_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			fb_w, fb_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GLuint fbo = 0;

		std::cout << "Framebuffer size: " << fb_w << "x" << fb_h << std::endl;

		glCreateFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, uv_tex, 0);
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, buffers);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cout << "Framebuffer initialization failed" << std::endl;
			glfwTerminate();
			return EXIT_FAILURE;
		}

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		std::vector<uint32_t> pixels;
		pixels.resize(fb_w * fb_h);

		std::random_device rd;
		std::mt19937 gen(rd());

		unsigned i = 0;
		while (i < n && !glfwWindowShouldClose(w))
		{
			if (glfwGetKey(w, GLFW_KEY_ESCAPE) != GLFW_RELEASE)
				glfwSetWindowShouldClose(w, GLFW_TRUE);

			glfwSetWindowTitle(w, ("[" + std::to_string(i) + "/" + std::to_string(n) + "]").c_str());
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glDrawBuffers(2, buffers);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			std::uniform_real_distribution<float> rdis(min_radius, max_radius);
			float radius = rdis(rd);

			std::uniform_real_distribution<float> fdis(glm::radians(min_fov), glm::radians(max_fov));
			float fov = fdis(rd);

			std::uniform_real_distribution<float> adis(0.0f, 360.0f);

			glm::mat4 rotate = glm::yawPitchRoll(glm::radians(adis(rd)), glm::radians(adis(rd)), glm::radians(adis(rd)));
			glm::mat4 view = glm::translate(glm::mat4(1.0), glm::vec3(0, 0, -radius)) * rotate;
			glm::mat4 proj = glm::perspectiveFov(fov, float(width), float(height), 0.01f, 200.0f);

			glm::mat4 VP = proj * view * glm::scale(glm::mat4(1.0), glm::vec3(0.5));

			glUseProgram(prog);
			glUniformMatrix4fv(glGetUniformLocation(prog, "MVP"), 1, false, &VP[0][0]);
			glBindVertexArray(vao);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tex);
			glDrawArrays(GL_TRIANGLES, 0, mesh.size());
			glBindVertexArray(0);
			glUseProgram(0);

			{
				std::experimental::filesystem::create_directory(out);

				std::string base = "";

				base += "S" + std::to_string(i);
				base += "F" + std::to_string(fov);
				base += "R" + std::to_string(radius);

				std::replace(base.begin(), base.end(), '.', '_');

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, color_tex);
				glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				stbi_write_png((out + "/" + base + "_color.png").c_str(), fb_w, fb_h, 4, pixels.data(), fb_w * 4);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, uv_tex);
				glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				stbi_write_png((out + "/" + base + "_uv.png").c_str(), fb_w, fb_h, 4, pixels.data(), fb_w * 4);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, fb_w, fb_h, 0, 0, fb_w, fb_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

			glfwPollEvents();
			glfwSwapBuffers(w);
			i++;
		}

		glfwTerminate();
	}
	catch (const cxxopts::OptionException& e)
	{
		std::cout << "Error parsing options: " << e.what() << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
	}

	return EXIT_SUCCESS;
}