<template>
  <div>
    <v-card>
      <v-card-title>
        BiliMessenger {{ getVersion() }}
      </v-card-title>
      <v-card-subtitle>
        舰长私信助手 @Xinrea
      </v-card-subtitle>
      <v-card-text>
        <p>
          <v-chip
            class="pointer"
            @click="open('https://github.com/Xinrea')"
          >
            GitHub
          </v-chip>
          <v-chip
            color="#fb7299"
            class="pointer"
            @click="open('https://space.bilibili.com/475210')"
          >
            Bilibili
          </v-chip>
          <v-chip
            color="#17a2b8"
            class="pointer"
            @click="open('https://joi-club.cn')"
          >
            Joi Club
          </v-chip>
        </p>
        <p />
        <v-btn
          color="orange"
          @click="open('https://space.bilibili.com/61639371/')"
        >
          关注 @轴伊Joi_Channel
        </v-btn>
      </v-card-text>
    </v-card>
  </div>
</template>

<script>
import {app} from '@electron/remote'
export default {
  name: 'AboutPage',
  methods: {
    open (link) {
      this.$electron.shell.openExternal(link)
    },
    getVersion () {
      return app.getVersion()
    }
  }
}
</script>

<style scoped>
.v-btn {
  text-transform: none;
}
</style>