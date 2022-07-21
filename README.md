# This — is The **Construct**

[![Chat in #construct:zemos.net](https://img.shields.io/matrix/construct:zemos.net.svg?label=Chat%20in%20%23construct%3Azemos.net&logo=matrix&server_fqdn=matrix.org&style=for-the-badge&color=8891CD)](https://matrix.to/#/#construct:zemos.net) [![](https://img.shields.io/badge/License-BSD-8891CD.svg?label=%20license&style=for-the-badge)]() [![](https://img.shields.io/badge/PRs-welcome-8891CD.svg?label=contributions&style=for-the-badge)]()

### Getting Started

[![](https://img.shields.io/badge/github-source-8891CD.svg?logo=GitHub&style=for-the-badge)](https://github.com/matrix-construct/construct) [![](https://img.shields.io/badge/docker-images-8891CD.svg?logo=Docker&style=for-the-badge)](https://registry.hub.docker.com/r/jevolk/construct)

- `git clone https://github.com/matrix-construct/construct`
- `docker pull jevolk/construct:ubuntu-22.04-built`

1. See the [BUILD](https://github.com/matrix-construct/construct/wiki/BUILD) instructions to compile Construct from source.

2. See the [SETUP](https://github.com/matrix-construct/construct/wiki/SETUP) instructions to run Construct for the first time.

3. See the [TUNING](https://github.com/matrix-construct/construct/wiki/TUNING) guide to optimize Construct for your deployment.

##### TROUBLESHOOTING

See the [TROUBLESHOOTING](https://github.com/matrix-construct/construct/wiki/Troubleshooting-problems) guide for solutions to possible
problems.

See the [FREQUENTLY ASKED QUESTIONS](https://github.com/matrix-construct/construct/wiki/FAQ) for answers to the most common
perplexities.

## Developers

[![](https://ci.appveyor.com/api/projects/status/qck2bpb57704jmtf?svg=true)]()

##### DOCUMENTATION

Generate doxygen using `doxygen ./Doxyfile` the target
directory is `doc/html`. Browse to `doc/html/index.html`.

##### DEPLOYMENT ROADMAP

```
🛑 Operating a Construct server which is open to public user registration is unsafe. Local users may
be able to exceed resource limitations and deny service to other users.
```

- [x] **Personal**: Dozens of users. Few default restrictions; higher log output.
- [ ] **Company**: Hundreds of users. Moderate default restrictions.
- [ ] **Public**: Thousands of users. Untrusting configuration defaults.

> Due to the breadth of the Matrix client/server protocol we can only endorse
production use of Construct gradually while local user restrictions are
developed. This notice applies to locally registered users connecting with
clients, it does not apply to federation.
