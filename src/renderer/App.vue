<template>
<v-app>
    <v-navigation-drawer app permanent :mini-variant.sync="mini" v-model="drawer">
        <v-list-item>
            <v-list-item-content>
                <v-list-item-title class="text-h6">
                    BiliMessenger
                </v-list-item-title>
                <v-list-item-subtitle>
                    Bilibili舰长私信助手 @Xinrea
                </v-list-item-subtitle>
            </v-list-item-content>
            <v-btn icon @click.stop="mini = !mini">
                <v-icon>mdi-chevron-left</v-icon>
            </v-btn>
        </v-list-item>
        <v-divider></v-divider>
        <v-list
          nav
          dense
        >
          <v-list-item link @click="RouteTo('/')">
            <v-list-item-icon>
              <v-icon>mdi-home-circle-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>总览</v-list-item-title>
          </v-list-item>
          <v-list-item link>
            <v-list-item-icon>
              <v-icon>mdi-format-list-bulleted-square</v-icon>
            </v-list-item-icon>
            <v-list-item-title>舰长列表</v-list-item-title>
          </v-list-item>
          <v-list-item link>
            <v-list-item-icon>
              <v-icon>mdi-file-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>模板管理</v-list-item-title>
          </v-list-item>
          <v-list-item link>
            <v-list-item-icon>
              <v-icon>mdi-gift-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>兑换码管理</v-list-item-title>
          </v-list-item>
          <v-list-item link>
            <v-list-item-icon>
              <v-icon>mdi-message-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>私信群发</v-list-item-title>
          </v-list-item>
          <v-list-item link @click="RouteTo('/setting')">
            <v-list-item-icon>
              <v-icon>mdi-cog-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>设置</v-list-item-title>
          </v-list-item>
          <v-list-item link>
            <v-list-item-icon>
              <v-icon>mdi-information-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>关于</v-list-item-title>
          </v-list-item>
        </v-list>
    </v-navigation-drawer>

    <!-- 根据应用组件来调整你的内容 -->
    <v-main>

        <!-- 给应用提供合适的间距 -->
        <v-container fluid>

            <!-- 如果使用 vue-router -->
            <router-view :isSet="isSet" :userdata="userdata" v-on:login-success="onLogin"></router-view>
        </v-container>
    </v-main>
</v-app>
</template>

<script>
import ipcRenderer from 'electron'
export default {
  name: 'bilimessenger',
  data () {
    return {
      drawer: true,
      mini: false,
      overlay: true,
      dialog: true,
      isSet: false,
      userdata: null
    }
  },
  mounted () {
  },
  methods: {
    RouteTo: function (path) {
      if (this.$route.path !== path) {
        this.$router.push(path)
      }
    },
    onLogin: function (params) {
      this.isSet = true
      this.userdata = params
      ipcRenderer.invoke('setStoreValue', 'logindata', params)
      console.log('login data stored')
    }
  }
}
</script>

<style>
/* CSS */
</style>
