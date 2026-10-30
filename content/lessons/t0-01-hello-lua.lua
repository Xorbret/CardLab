return {
  id = "t0-01-hello-lua",
  track = 0,
  title = "Hello, Lua",
  permissions = {},
  pages = {
    "CardLab lessons follow one loop: Learn, Build, Test, Break, Defend. You read a little, write a little code, and the device checks your work.",
    "Code here is Lua. print() shows text. Variables need no type: local name = \"Robert\". Strings join with two dots: \"Hi \" .. name.",
    "Your code runs in a sandbox: it can only touch the hardware a lesson allows, and a runaway loop is stopped after a few seconds.",
  },
  task = "Print the text: hello, cardlab",
  starter = [[print("hello")]],
  check = function(r)
    if not r.ok then return false, "Your code hit an error: " .. r.error end
    if r.output:lower():find("hello, cardlab", 1, true) then
      return true, "Nice. That is your first program on the device."
    end
    return false, "Close. Print exactly: hello, cardlab"
  end,
  quiz = {
    { q = "Which joins two strings in Lua?", choices = { "+", "..", "&" }, answer = 2,
      why = "Lua uses .. for concatenation; + is only for numbers." },
  },
  reflect = "A user asks why their smart plug app says 'firmware'. How would you explain firmware in one sentence?",
}
