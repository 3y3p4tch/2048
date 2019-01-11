import random
import tkinter as tk
import tkinter.ttk as ttk
from copy import deepcopy

class GameLoseException(Exception):
	def __init__(self, message):
		super().__init__(message)

class Grid:
	def __init__(self, rows = 3, cols = 3):
		self.rows = rows
		self.cols = cols
		self.matrix = []
		self.score = 0
		for i in range(rows):
			self.matrix.append([0] * cols)
		
		empty_pos = [(i, j) for i in range(len(self.matrix)) for j in range(len(self.matrix[i])) if self.matrix[i][j] == 0]
		move = random.choices([1, 2], [3, 1])[0]  #1 -> 2 -> 75% probab, 2 -> 4 -> 25% probab
		pos = random.choice(empty_pos)
		self.matrix[pos[0]][pos[1]] = move

	def move(self, direction):
		prev_matrix = deepcopy(self.matrix)
		if direction == 0: #up
			for c in range(self.cols):
				lst = [r[c] for r in self.matrix if r[c]]
				k = 0
				while k < len(lst) - 1:
					if lst[k] == lst[k + 1]:
						self.score += 2**lst[k]
						lst[k] += 1
						del lst[k + 1]
					k += 1
				for r in range(self.rows):
					self.matrix[r][c] = lst[r] if r < len(lst) else 0
		
		if direction == 1: #down
			for c in range(self.cols):
				lst = [r[c] for r in self.matrix if r[c]]
				k = len(lst) - 1
				while k > 0:
					if lst[k] == lst[k - 1]:
						self.score += 2**lst[k]
						lst[k] += 1
						del lst[k - 1]
						k -= 1
					k -= 1
				for r in range(self.rows):
					self.matrix[r][c] = lst[len(lst) + r - self.rows] if r + len(lst) >= self.rows else 0
					
		if direction == 2: #left
			for r in range(self.rows):
				lst = [c for c in self.matrix[r] if c]
				k = 0
				while k < len(lst) - 1:
					if lst[k] == lst[k + 1]:
						self.score += 2**lst[k]
						lst[k] += 1
						del lst[k + 1]
					k += 1
				self.matrix[r] = lst + [0] * (self.cols - len(lst))
				
		if direction == 3: #right
			for r in range(self.rows):
				lst = [c for c in self.matrix[r] if c]
				k = len(lst) - 1
				while k > 0:
					if lst[k] == lst[k - 1]:
						self.score += 2**lst[k]
						lst[k] += 1
						del lst[k - 1]
						k -= 1
					k -= 1
				self.matrix[r] = [0] * (self.cols - len(lst)) + lst
		
		empty_pos = [(i, j) for i in range(len(self.matrix)) for j in range(len(self.matrix[i])) if self.matrix[i][j] == 0]
		if self.matrix != prev_matrix and empty_pos:
			move = random.choices([1, 2], [3, 1])[0]  #1 -> 2 -> 75% probab, 2 -> 4 -> 25% probab
			pos = random.choice(empty_pos)
			self.matrix[pos[0]][pos[1]] = move
		elif not empty_pos:
			raise GameLoseException('You lose')

	def __str__(self):
		_str = 'Score: ' + str(self.score) + '\n'
		for r in self.matrix:
			for c in r[:-1]:
				_str += str(c) + ' '
			_str += str(r[-1]) + '\n'
		return _str[:-1]
		 
class Board:
	colors = ['#ddd', '#ffd54f', '#ff8f00', '#ff6f00', '#f44336', '#e74c3c', '#c0392b', '#8bc34a', '#4caf50', '#3f51b5', '#673ab7', '#e91e63', '#000']

	def __init__(self, parent, rows = 3, cols = 3):
		self.grid = Grid(rows, cols)
		self.parent = parent
		f1 = tk.Frame(self.parent)
		f2 = tk.Frame(f1)
		self.parent.bind('<Up>', lambda e: self.update(0))
		self.parent.bind('<Down>', lambda e: self.update(1))
		self.parent.bind('<Left>', lambda e: self.update(2))
		self.parent.bind('<Right>', lambda e: self.update(3))
		self.scoreboard = tk.Label(f1, text=self.grid.score, font='Times 24', bg='coral', fg='white')
		self.scoreboard.pack(side=tk.TOP, ipadx=32, ipady=8)
		self.labels = [tk.Label(f2, text='Score: {}'.format(self.grid.score), fg='white', font='Jokerman 24', width=5) for i in range(self.grid.rows) for j in range(self.grid.cols)]
		for i in range(self.grid.rows):
			for j in range(self.grid.cols):
				self.labels[i * self.grid.rows + j].grid(row=i, column=j, ipadx=40, ipady=40, padx=1, pady=1)
		f2.pack(side=tk.BOTTOM)
		f1.pack(fill=tk.BOTH)
		self.update()
		
	def update(self, direction=None):
		try:
			if direction is not None: self.grid.move(direction)
			for i in range(self.grid.rows):
				for j in range(self.grid.cols):
					self.labels[i * self.grid.rows + j].config(text=2 ** self.grid.matrix[i][j] if self.grid.matrix[i][j] else '', bg=Board.colors[self.grid.matrix[i][j]])
			self.scoreboard.config(text='Score: {}'.format(self.grid.score))
		except GameLoseException:
			self.parent.destroy()
		

if __name__ == '__main__':
	root = tk.Tk()
	brd = Board(root,5,5)
	root.mainloop()
