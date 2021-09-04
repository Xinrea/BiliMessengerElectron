<template>
  <div>
    <v-card
    elevation="2"
    >
    <v-card-title>未找到有效的设置</v-card-title>
    <v-card-text>请前往设置页面，设置私信所用的账号以及需要管理的直播间号</v-card-text>
    <v-card-actions>
        <v-btn
        elevation="1"
        @click="RouteTo('/setting')"
        >前往设置</v-btn>
    </v-card-actions>
    </v-card>
  </div>
</template>

<script>
export default {
  name: 'setting-alert-page',
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

</style>
