FROM node:22-slim AS build
WORKDIR /app

COPY package*.json .
#RUN npm cache clean --force
#RUN rm -rf node_modules package-lock.json
RUN npm ci

COPY . .
RUN npm run build
RUN npm rebuild better-sqlite3
RUN npm prune --production

FROM node:22-slim
RUN apt-get update && apt-get install -y curl

WORKDIR /app

COPY --from=build /app/build build/
COPY --from=build /app/node_modules node_modules/
COPY package.json .

# Use non-root user
USER node

EXPOSE 3000
ENV NODE_ENV=production
ENTRYPOINT ["node", "build"]