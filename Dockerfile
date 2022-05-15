# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.224.3/containers/alpine/.devcontainer/base.Dockerfile

ARG VARIANT="latest"
FROM gbenm/pintos:${VARIANT}

# ** [Optional] Uncomment this section to install additional packages. **
# RUN apk update \
#     && apk add --no-cache <your-package-list-here>
