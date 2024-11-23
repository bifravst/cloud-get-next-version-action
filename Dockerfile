FROM node:22-alpine
# Needed for semantic release
RUN apk add --no-cache git
RUN npm -g i npm@
WORKDIR /get-next-version
COPY get-next-version.ts /get-next-version
COPY package.json /get-next-version
COPY package-lock.json /get-next-version
COPY tsconfig.json /get-next-version
RUN npm ci && npx tsc
ENV DEBUG='*'
ENTRYPOINT ["node","--unhandled-rejections=strict","/get-next-version/dist/get-next-version.js"]