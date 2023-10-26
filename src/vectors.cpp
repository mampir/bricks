/* Bricks Game - Vector Functions
 *
 * Copyright (C) 2017 LibTec
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

struct V2 {
  float x, y;
};

V2
operator+ (V2 a, V2 b)
{
  a.x += b.x;
  a.y += b.y;
  return a;
}

V2
operator+= (V2 &a, V2 b)
{
  a.x += b.x;
  a.y += b.y;
  return a;
}

V2
operator/ (V2 a, V2 b)
{
  a.x /= b.x;
  a.y /= b.y;
  return a;
}

V2
operator/= (V2 &a, V2 b)
{
  a.x /= b.x;
  a.y /= b.y;
  return a;
}

V2
operator- (V2 a, V2 b)
{
  a.x -= b.x;
  a.y -= b.y;
  return a;
}

V2
operator-= (V2 &a, V2 b)
{
  a.x -= b.x;
  a.y -= b.y;
  return a;
}

V2
operator* (V2 v, float n)
{
  v.x *= n;
  v.y *= n;
  return v;
}

V2
operator*= (V2 &v, float n)
{
  v.x *= n;
  v.y *= n;
  return v;
}

V2
operator/ (V2 v, float n)
{
  v.x /= n;
  v.y /= n;
  return v;
}

V2
operator/= (V2 &v, float n)
{
  v.x /= n;
  v.y /= n;
  return v;
}

V2
normalize (V2 v)
{
  float length = sqrt (v.x*v.x + v.y*v.y);
  v.x /= length;
  v.y /= length;
  return v;
}
