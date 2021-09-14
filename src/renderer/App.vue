<template>
  <v-app>
    <v-navigation-drawer
      v-model="drawer"
      app
      permanent
      :mini-variant.sync="mini"
    >
      <v-list-item>
        <v-list-item-content>
          <v-list-item-title class="text-h6">
            BiliMessenger
          </v-list-item-title>
          <v-list-item-subtitle>
            舰长私信助手
          </v-list-item-subtitle>
        </v-list-item-content>
        <v-btn
          icon
          @click.stop="mini = !mini"
        >
          <v-icon>mdi-chevron-left</v-icon>
        </v-btn>
      </v-list-item>
      <v-divider />
      <v-list
        nav
        dense
      >
        <v-list-item-group
          v-model="selectedItem"
        >
          <v-list-item
            link
            @click="RouteTo('/')"
          >
            <v-list-item-icon>
              <v-icon>mdi-home-circle-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>总览</v-list-item-title>
          </v-list-item>
          <v-list-item
            link
            @click="RouteTo('/guard_list')"
          >
            <v-list-item-icon>
              <v-icon>mdi-format-list-bulleted-square</v-icon>
            </v-list-item-icon>
            <v-list-item-title>舰长列表</v-list-item-title>
          </v-list-item>
          <v-list-item
            link
            @click="RouteTo('/template')"
          >
            <v-list-item-icon>
              <v-icon>mdi-file-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>模板管理</v-list-item-title>
          </v-list-item>
          <v-list-item
            link
            @click="RouteTo('/code')"
          >
            <v-list-item-icon>
              <v-icon>mdi-gift-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>兑换码管理</v-list-item-title>
          </v-list-item>
          <v-list-item
            link
            @click="RouteTo('/message')"
          >
            <v-list-item-icon>
              <v-icon>mdi-message-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>私信群发</v-list-item-title>
          </v-list-item>
          <v-list-item
            link
            @click="RouteTo('/setting')"
          >
            <v-list-item-icon>
              <v-icon>mdi-cog-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>设置</v-list-item-title>
          </v-list-item>
          <v-list-item
            link
            @click="RouteTo('/about')"
          >
            <v-list-item-icon>
              <v-icon>mdi-information-outline</v-icon>
            </v-list-item-icon>
            <v-list-item-title>关于</v-list-item-title>
          </v-list-item>
        </v-list-item-group>
      </v-list>
    </v-navigation-drawer>

    <!-- 根据应用组件来调整你的内容 -->
    <v-main>
      <!-- 给应用提供合适的间距 -->
      <v-container
        fluid
        style="height: 100%"
      >
        <!-- 如果使用 vue-router -->
        <keep-alive>
          <router-view
            v-if="$route.meta.keepAlive"
          />
        </keep-alive>
        <router-view
          v-if="!$route.meta.keepAlive"
        />
      </v-container>
    </v-main>
  </v-app>
</template>

<script>
export default {
  name: 'Bilimessenger',
  data () {
    return {
      drawer: true,
      mini: false,
      overlay: true,
      dialog: true,
      selectedItem: 0
    }
  },
  mounted () {
  },
  methods: {
    RouteTo: function (path) {
      if (this.$route.path !== path) {
        this.$router.push(path)
      }
    }
  }
}
</script>

<style>
::-webkit-scrollbar
{
  width: 5px;
}

/*定义滚动条轨道 内阴影+圆角*/
::-webkit-scrollbar-track
{
  border-radius: 10px;
  background-color: rgba(255, 255, 255, 0.34);
}

/*定义滑块 内阴影+圆角*/
::-webkit-scrollbar-thumb
{
  border-radius: 10px;
  -webkit-box-shadow: inset 0 0 6px rgba(0,0,0,.3);
  background-color: rgba(141, 96, 56, 0.1);
}
</style>
