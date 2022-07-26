# This — is The **Construct**

[![Chat in #construct:zemos.net](https://img.shields.io/matrix/construct:zemos.net.svg?label=Chat%20in%20%23construct%3Azemos.net&logo=matrix&server_fqdn=matrix.org&style=for-the-badge&color=5965AF)](https://matrix.to/#/#construct:zemos.net) [![](https://img.shields.io/badge/License-BSD-5965AF.svg?label=%20license&style=for-the-badge)]()

### 📦 GETTING STARTED

[![](https://img.shields.io/badge/github-source-8891CD.svg?logo=GitHub&style=for-the-badge)](https://github.com/matrix-construct/construct) [![](https://img.shields.io/badge/docker-images-8891CD.svg?logo=Docker&style=for-the-badge)](https://registry.hub.docker.com/r/jevolk/construct)

|     |    |
|:---:|:---|
| [![](https://img.shields.io/docker/image-size/jevolk/construct/alpine-3.16-base-built.svg?label=alpine3.16-base&logo=Docker&style=flat-square&color=5965AF&logoWidth=38)](https://registry.hub.docker.com/r/jevolk/construct/tags) | `docker pull jevolk/construct:alpine-3.16-base-built` |
| [![](https://img.shields.io/docker/image-size/jevolk/construct/alpine-3.16-full-built.svg?label=alpine-3.16-full&logo=Docker&style=flat-square&color=5965AF&logoWidth=32)](https://registry.hub.docker.com/r/jevolk/construct/tags) | `docker pull jevolk/construct:alpine-3.16-full-built` |
| [![](https://img.shields.io/docker/image-size/jevolk/construct/ubuntu-22.04-built.svg?label=ubuntu-22.04&logo=Docker&style=flat-square&color=5965AF&logoWidth=46)](https://registry.hub.docker.com/r/jevolk/construct/tags) | `docker pull jevolk/construct:ubuntu-22.04-built` |

- `git clone https://github.com/matrix-construct/construct`

	[![](https://img.shields.io/github/repo-size/matrix-construct/construct.svg?logo=GitHub&style=flat-square&color=5965AF)](https://github.com/matrix-construct/construct)
	[![](https://img.shields.io/github/languages/code-size/matrix-construct/construct.svg?logo=GitHub&style=flat-square&color=5965AF)](https://github.com/matrix-construct/construct)
	[![](https://img.shields.io/github/directory-file-count/matrix-construct/construct.svg?type=dir&label=directories&logo=GitHub&style=flat-square&color=5965AF)](https://github.com/matrix-construct/construct)
	[![](https://img.shields.io/github/directory-file-count/matrix-construct/construct.svg?type=file&label=files&logo=GitHub&style=flat-square&color=5965AF)](https://github.com/matrix-construct/construct)


### 🗒️ INSTRUCTIONS

1. 🏗️  [BUILD](https://github.com/matrix-construct/construct/wiki/BUILD) instructions to compile Construct from source.

2. 🪛 [SETUP](https://github.com/matrix-construct/construct/wiki/SETUP) instructions to run Construct for the first time.

3. ⚡ [TUNING](https://github.com/matrix-construct/construct/wiki/TUNING) guide to optimize Construct for your deployment.

🙋 [TROUBLESHOOTING](https://github.com/matrix-construct/construct/wiki/Troubleshooting-problems) guide for solutions to possible problems.

❓ [FREQUENTLY ASKED QUESTIONS](https://github.com/matrix-construct/construct/wiki/FAQ) for answers to the most common perplexities.

>🛑 Operating a Construct server which is open to public user registration is unsafe. Local users may be able to exceed resource limitations and deny service to other users.

## 👷‍♀️ DEVELOPERS

[![](https://img.shields.io/badge/PRs-welcome-8891CD.svg?label=contributions)]() [![](https://ci.appveyor.com/api/projects/status/qck2bpb57704jmtf?svg=true&style=for-the-badge)]()

##### 📚 DOCUMENTATION

Generate doxygen using `doxygen ./Doxyfile` the target directory is `doc/html`.
Browse to `doc/html/index.html`.

##### 🛣️ DEPLOYMENT ROADMAP

- [x] **Personal**: Dozens of users. Few default restrictions; higher log output.
- [ ] **Company**: Hundreds of users. Moderate default restrictions.
- [ ] **Public**: Thousands of users. Untrusting configuration defaults.

> Due to the breadth of the Matrix client/server protocol we can only endorse
production use of Construct gradually while local user restrictions are
developed. This notice applies to locally registered users connecting with
clients, it does not apply to federation.
