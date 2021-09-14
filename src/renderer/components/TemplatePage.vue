<template>
  <v-row
    style="height: 100%"
  >
    <v-col>
      <v-card>
        <v-card-title>
          模板
        </v-card-title>
        <v-card-text>
          <v-combobox
            v-model="templateSelect"
            :items="templateList"
            label="模板名"
            @change="tempNameChange"
          />
          <v-textarea
            v-model="template.value"
            label="模板内容"
            auto-grow
            filled
            @change="tempValueChange"
          />
          <v-btn
            color="primary"
            :disabled="!changed"
            @click="saveTemplate"
          >
            保存
          </v-btn>
          <v-btn
            color="rgb(195, 82, 82)"
            @click="deleteTemplate"
          >
            删除
          </v-btn>
          <v-divider
            class="mt-6"
          />
          <div
            class="mt-6"
          >
            <p>
              <v-btn
                v-for="tag in tags"
                :key="tag"
                rounded
                small
                color="rgb(212, 163, 64)"
                class="ma-1"
                @click="addTag('{'+tag+'}')"
              >
                {{ '{'+tag+'}' }}
              </v-btn>
            </p>
            <p class="text-body-2">
              可在编写模板内容时手动输入 tag， 或是点击以上 tag 按钮快捷添加。
            </p>
            <p class="text-body-2">
              模板作为消息被发送时，其中包含的 tag 将会被替换为具体内容；替换后的效果可参考右侧预览。
            </p>
            <p class="text-body-2">
              {level}: 总督/提督/舰长/自定义
            </p>
            <p class="text-body-2">
              {name}: 用户名
            </p>
            <p class="text-body-2">
              {code}: 兑换码
            </p>
            <p class="text-body-2">
              {date}: 日期
            </p>
            <p class="text-body-2">
              {month}: 月份
            </p>
            <p class="text-body-2">
              {lastMonth}: 上个月的月份
            </p>
            <p class="text-body-2">
              {day}: 天
            </p>
          </div>
        </v-card-text>
      </v-card>
    </v-col>
    <v-col>
      <v-card>
        <v-card-title>
          预览
        </v-card-title>
        <v-card-text
          class="d-flex flex-row"
        >
          <v-avatar
            color="primary"
            size="38"
            class="mr-2"
          >
            You
          </v-avatar>
          <v-textarea
            background-color="rgb(69, 69, 69)"
            no-resize
            readonly
            outlined
            class="rounded-tl-0 rounded-bl-lg rounded-br-lg rounded-tr-lg"
            auto-grow
            rows="1"
            :value="processTag(template.value)"
          />
        </v-card-text>
      </v-card>
    </v-col>
  </v-row>
</template>

<script>
export default {
  name: 'TemplatePage',
  data () {
    return {
      templateSelect: "",
      templateList: [],
      template: {
        value: ""
      },
      changed: false,
      tags: ['level', 'name', 'code', 'date', 'month', 'lastMonth', 'day']
    }
  },
  mounted () {
    this.templateList = this.Store.get('templateList',[])
  },
  methods: {
    saveTemplate() {
      console.log(this.templateSelect, this.template.value)
      if (this.templateSelect === '') {
        this.templateSelect = '未命名'
      }
      if (!this.templateList.includes(this.templateSelect)) {
        this.templateList.push(this.templateSelect)
        this.Store.set('templateList', this.templateList)
      }
      this.Store.set('template:'+this.templateSelect, this.template.value)
      this.changed = false
    },
    deleteTemplate() {
      console.log('Delete Template', this.templateSelect, this.templateList)
      let index = this.templateList.indexOf(this.templateSelect)
      if (index > -1) {
        this.templateList.splice(index,1)
        this.Store.set('templateList', this.templateList)
        this.Store.delete('template:'+this.templateSelect)
        this.templateSelect = ''
        this.template.value = ''
      }
    },
    tempNameChange() {
      console.log(this.templateSelect)
      if (this.templateList.includes(this.templateSelect)) {
        this.template.value = this.Store.get('template:'+this.templateSelect, '')
      }
    },
    tempValueChange() {
      this.changed = true
    },
    processTag(v) {
      let dates = this.getCurrentDate()
      return v.replaceAll('{level}', '总督').replaceAll('{name}', 'Xinrea')
        .replaceAll('{code}', 'https://afdian.net/redeem/some_code_link')
        .replaceAll('{date}', dates.fullDate)
        .replaceAll('{month}', dates.month.toString())
        .replaceAll('{lastMonth}', dates.lastMonth.toString())
        .replaceAll('{day}', dates.day.toString())
    },
    getCurrentDate() {
      let date = new Date();
      let mon = date.getMonth() + 1;
      let day = date.getDate();
      return {
        fullDate: date.getFullYear() + "-" + (mon < 10 ? "0" + mon : mon) + "-" + (day < 10 ? "0" + day : day),
        month: date.getMonth()+1,
        lastMonth: date.getMonth() === 0 ? 12 : date.getMonth(),
        day: date.getDate()
      }
    },
    addTag(t) {
      this.template.value += t
    }
  }
}
</script>

<style scoped>
.v-btn {
  text-transform: none;
}
</style>