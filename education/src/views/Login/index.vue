<script setup lang="ts">
import { reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
// 假设您已经按照之前的建议封装了 api 请求
// import { loginApi } from '@/api/auth' 

const router = useRouter()
const loginFormRef = ref()
const loading = ref(false)

// 1. 数据模型
const form = reactive({
  email: '',
  password: ''
})

// 2. 验证规则 (团队开发规范)
const rules = {
  email: [
    { required: true, message: '请输入学校邮箱', trigger: 'blur' },
    { type: 'email', message: '邮箱格式不正确', trigger: 'blur' }
  ],
  password: [
    { required: true, message: '请输入密码', trigger: 'blur' }
  ]
}

// 3. 登录逻辑
const handleLogin = async () => {
  if (!loginFormRef.value) return
  
  await loginFormRef.value.validate(async (valid: boolean) => {
    if (valid) {
      loading.value = true
      try {
        console.log("提交参数:", { ...form })
        // 模拟 API 调用
        // const res = await loginApi(form)
        // localStorage.setItem('token', res.token)
        ElMessage.success('登录成功')
        router.push('/')
      } finally {
        loading.value = false
      }
    }
  })
}

const loginWithSSO = () => {
  console.log("使用 SSO 登录")
}
</script>

<template>
  <div class="login-wrapper">
    <main class="frame">
      <section class="hero">
        <div class="badge">
          <span class="badge-dot"></span>
          下一代学习空间
        </div>
        <h1>欢迎回到 Elm Academy</h1>
        <p class="lede">
          以项目为驱动的课程、实时辅导与学习社区，让你在最短时间内掌握硬技能。
          登录后继续未完的课程，或加入正在进行的现场课堂。
        </p>
        <div class="pill-list">
          <span class="pill">个性化路径</span>
          <span class="pill">直播助教</span>
          <span class="pill">课程证书</span>
          <span class="pill">团队看板</span>
        </div>
      </section>

      <section class="card">
        <div class="card-header">
          <h2>登录你的课堂</h2>
          <p>使用学校邮箱或单点登录。</p>
        </div>

        <el-form 
          ref="loginFormRef"
          :model="form" 
          :rules="rules"
          label-position="top"
          class="custom-form"
          hide-required-asterisk
        >
          <el-form-item label="学校邮箱" prop="email">
            <el-input 
              v-model.trim="form.email" 
              placeholder="you@campus.edu" 
              class="custom-input"
            />
          </el-form-item>

          <el-form-item label="密码" prop="password">
            <el-input 
              v-model="form.password" 
              type="password" 
              placeholder="********" 
              show-password
              class="custom-input"
            />
          </el-form-item>

          <div class="action action-between">
            <a href="#" class="forgot-link">忘记密码？</a>
            <button class="btn-primary" type="button" @click="handleLogin" :disabled="loading">
              {{ loading ? '进入中...' : '进入课堂' }}
            </button>
          </div>
        </el-form>

        <div class="divider">或</div>
        
        <button class="btn-ghost" type="button" @click="loginWithSSO">
          使用校园单点登录
        </button>
        
        <p class="note">还没有账号？ <a class="note-link" href="#">创建学习者档案</a></p>
      </section>
    </main>
  </div>
</template>

<style scoped>
/* 定义变量，确保与原设计一致 */
.login-wrapper {
  --ink: #0b1021;
  --teal: #15c0a0;
  --amber: #f8c361;
  --stroke: rgba(255, 255, 255, 0.08);
  --shadow: 0 20px 60px rgba(11, 16, 33, 0.35);
  
  width: 100%;
  min-height: 100vh;
  display: grid;
  place-items: center;
  padding: 32px;
  background:
    radial-gradient(140% 120% at 20% 25%, rgba(21, 192, 160, 0.25), transparent 45%),
    radial-gradient(120% 120% at 85% 20%, rgba(248, 195, 97, 0.18), transparent 50%),
    #0b1021;
}

/* 核心框架结构 */
.frame {
  width: min(1100px, 100%);
  min-height: 620px;
  background: linear-gradient(145deg, rgba(16, 24, 54, 0.92), rgba(8, 12, 28, 0.96));
  border: 1px solid var(--stroke);
  border-radius: 28px;
  box-shadow: var(--shadow);
  overflow: hidden;
  display: grid;
  grid-template-columns: 1.1fr 0.9fr;
}

/* 左侧 Hero 样式复刻 */
.hero {
  position: relative;
  padding: 56px;
  overflow: hidden;
  background: linear-gradient(160deg, rgba(21, 192, 160, 0.18), rgba(248, 195, 97, 0.06));
}

.hero::after, .hero::before {
  content: "";
  position: absolute;
  filter: blur(60px);
  opacity: 0.35;
}

.hero::after {
  width: 220px; height: 220px; top: 12%; right: 8%;
  background: radial-gradient(circle, rgba(248, 195, 97, 0.8), transparent 60%);
  animation: float 9s ease-in-out infinite;
}

.hero::before {
  width: 280px; height: 280px; bottom: -8%; left: 10%;
  background: radial-gradient(circle, rgba(21, 192, 160, 0.9), transparent 65%);
  animation: float 11s ease-in-out infinite reverse;
}

.badge {
  display: inline-flex; align-items: center; gap: 10px; padding: 10px 16px; border-radius: 999px;
  background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.12);
  backdrop-filter: blur(6px); font-weight: 600; font-size: 14px; color: #fff;
}

.badge-dot { width: 10px; height: 10px; border-radius: 50%; background: var(--amber); }

h1 { margin: 26px 0 14px; font-size: clamp(32px, 3vw + 12px, 46px); line-height: 1.1; color: #fff; }

.lede { color: #d7def5; max-width: 480px; font-size: 17px; line-height: 1.6; margin: 0 0 38px; }

.pill-list { display: flex; flex-wrap: wrap; gap: 12px; }
.pill {
  padding: 10px 14px; border-radius: 12px; font-weight: 600; color: #f5f8ff;
  background: rgba(255, 255, 255, 0.06); border: 1px solid rgba(255, 255, 255, 0.12);
}

/* 右侧 Card 样式复刻 */
.card {
  padding: 52px 48px; background: rgba(9, 12, 24, 0.9); backdrop-filter: blur(10px);
  border-left: 1px solid var(--stroke); display: flex; flex-direction: column; gap: 22px; justify-content: center;
}

.card h2 { margin: 0; font-size: 26px; color: #fff; }
.card p { margin: 0; color: #c7cee5; }

/* 重点：Element Plus 样式穿透修复 */
:deep(.el-form-item__label) {
  color: #fff !important; font-weight: 700; padding: 0 !important; margin-bottom: 6px !important;
}

:deep(.el-input__wrapper) {
  background: rgba(255, 255, 255, 0.04) !important;
  box-shadow: none !important; border: 1px solid rgba(255, 255, 255, 0.08) !important;
  border-radius: 14px; padding: 8px 16px; height: 48px; transition: all 0.2s;
}

:deep(.el-input__wrapper.is-focus) {
  border-color: rgba(21, 192, 160, 0.7) !important;
  box-shadow: 0 0 0 4px rgba(21, 192, 160, 0.15) !important;
}

:deep(.el-input__inner) { color: #f5f8ff !important; font-size: 15px; }

/* 按钮样式复刻 */
.btn-primary {
  background: linear-gradient(135deg, var(--teal), #12a98c);
  color: #041020; border: none; border-radius: 14px; padding: 14px 24px;
  font-weight: 800; cursor: pointer; transition: all 0.2s;
  box-shadow: 0 12px 30px rgba(21, 192, 160, 0.35);
}

.btn-primary:hover { filter: brightness(1.05); transform: translateY(-1px); }

.btn-ghost {
  background: transparent; color: #f5f8ff; border: 1px solid rgba(255, 255, 255, 0.12);
  padding: 14px; border-radius: 14px; font-weight: 800; cursor: pointer; transition: all 0.2s;
}

.btn-ghost:hover { border-color: rgba(255, 255, 255, 0.2); transform: translateY(-1px); }

/* 其他辅助样式 */
.action { display: flex; align-items: center; margin-top: 12px; }
.action-between { justify-content: space-between; }
.forgot-link { color: var(--amber); font-weight: 700; text-decoration: none; }
.forgot-link:hover { text-decoration: underline; }

.divider {
  display: grid; align-items: center; grid-template-columns: 1fr auto 1fr; gap: 12px;
  color: #8e97b5; font-weight: 700;
}
.divider::before, .divider::after { content: ""; height: 1px; background: rgba(255, 255, 255, 0.12); }

.note { color: #9cabc8; font-size: 14px; text-align: center; }
.note-link { color: var(--teal); font-weight: 700; text-decoration: none; }

/* 关键帧动画 */
@keyframes float {
  0%, 100% { transform: translateY(0) rotate(0.5deg); }
  50% { transform: translateY(-16px) rotate(-0.5deg); }
}

/* 响应式适配 */
@media (max-width: 960px) {
  .login-wrapper { padding: 18px; }
  .frame { grid-template-columns: 1fr; min-height: auto; }
  .hero { padding: 40px 32px; }
  .card { padding: 40px 32px 44px; }
}
</style>